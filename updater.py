import json
from posixpath import dirname
import sys
from packaging import version
import platform 
from collections import defaultdict
from pathlib import Path
import shutil
import tarfile
import requests 
import os

# Path to the file where updater stores the info on last checked release etc.
# The file MUST be writable
updater_info_path = "./updater.json"
# Destination path
dest_path = './games'
# Github repository to check for updates. Format: "owner/repo"
GITHUB_REPO = "thoth-tech/arcade-games"

def clear_file(fd):
    fd.truncate(0)
    fd.seek(0)
    

# Downgrading may be allowed with --allow-downgrade.
allow_downgrading = False

# This may be allowed with --allow-removing-destination-path
# Allows to remove the destination path if it is a file.
allow_removing_destination_path = False

def profile_platform(platform_title: str):
    if platform_title == 'linux':
        return "linux"
    elif platform_title == 'win32':
        return "win"
    elif platform_title == 'macos':
        return "macos"
    else:
        return None

# We are accepting everything compiled for 32-bit from repo
def profile_architecture(architecture_title: str):
    arch = platform.machine().lower()

    if "aarch64" in arch or "arm64" in arch:
        # return "arm64"
        return "arm"
    elif "arm" in arch:
        return "arm"
    elif "x86_64" in arch or "amd64" in arch or "x64" in arch:
        # return "x64"
        return "x86"
    elif "i386" in arch or "i686" in arch or "x86" in arch:
        return "x86"
    else:
        return None

def file_readable_to_updater(file_name: str):
    return file_name.endswith('.tar.gz')

def download_and_unpack(file_name: str, url: str, destination_path: str|Path):
    if file_name.endswith('.tar.gz'):
        response = requests.get(url, stream=True)
        if response.status_code == 200:
                with tarfile.open(fileobj=response.raw, mode='r:gz') as tar:
                    tar.extractall(path=destination_path)
        else:
            sys.stderr.write("Error: failed to download from the url " + url + ". Status code: " + str(response.status_code) + "\n")
    else:
        sys.stderr.write(f"Error: {file_name} is not a .tar.gz file and hence may not be unpacked here")

def unpack_games(available_games, info_file_contents: dict, destination_path, destination_architecture):
    for game_name, assets in available_games.items():
        assets_url, exec_url, assets_hash, exec_hash, assets_name, exec_name = (None,)*6
        if 'assets' in assets:
            assets_url = assets['assets']['url']
            assets_name = assets['assets']['name']
            assets_hash = assets['assets']['hash']
        if destination_architecture == 'x64':
            if 'x64' in assets:
                exec_url = assets['x64']['url']
                exec_name = assets['x64']['name']
                exec_hash = assets['x64']['hash']
            elif 'x86' in assets:
                sys.stderr.write("Warning: no x64 version found for the game " + game_name + ". Falling back to x86 version.\n")
                exec_url = assets['x86']['url']
                exec_name = assets['x86']['name']
                exec_hash = assets['x86']['hash']
            else: 
                sys.stderr.write("Warning: no x64 or x86 version found for the game " + game_name + ". Skipping it.\n")
        elif destination_architecture == 'arm64':
            if 'arm64' in assets:
                exec_url = assets['arm64']['url']
                exec_name = assets['arm64']['name']
                exec_hash = assets['arm64']['hash']
            elif 'arm' in assets:
                sys.stderr.write("Warning: no arm64 version found for the game " + game_name + ". Falling back to arm version.\n")
                exec_url = assets['arm']['url']
                exec_name = assets['arm']['name']
                exec_hash = assets['arm']['hash']
            else: 
                sys.stderr.write("Warning: no arm or arm64 version found for the game " + game_name + ". Skipping it.\n")
        elif destination_architecture == 'x86' or destination_architecture == 'arm':
            if destination_architecture in assets:
                exec_url = assets[destination_architecture]['url']
                exec_name = assets[destination_architecture]['name']
                exec_hash = assets[destination_architecture]['hash']
            else: 
                sys.stderr.write("Warning: no " + destination_architecture + " version found for the game " + game_name + ". Skipping it.\n")

        def check_file_needs_update(info_file_contents, game_name, file_name, file_hash, file_url):
            previous_hash = info_file_contents['files'].get(file_name, None)
            if previous_hash is not None and previous_hash == file_hash:
                sys.stderr.write(f"{file_name} is identical to the currently installed and may be skipped\n")
                return False
            else:
                info_file_contents['files'][ file_name ]  = file_hash
                return True


        # TODO: ensure to not pull two exact same files (check hash sum)
        if exec_url is not None:
            previous_hash = info_file_contents['files'].get(exec_name, None)
            exec_requires_update = False
            assets_requires_update = False

            exec_requires_update =  check_file_needs_update(info_file_contents, game_name, exec_name, exec_hash, exec_url)
            if assets_url is not None:
                assets_requires_update = check_file_needs_update(info_file_contents, game_name, assets_name, assets_hash, assets_url)
                
            # Update both regardless of which one in particular requires the update
            if exec_requires_update or assets_requires_update:
                end_game_path = Path(destination_path).joinpath('./' + game_name).resolve()
                # Before any download and unpack, we would like to wipe the directory of the game itself
                if Path(end_game_path).exists():
                    shutil.rmtree(end_game_path)

                download_and_unpack(exec_name, exec_url, end_game_path)
                if assets_url is not None:
                    download_and_unpack(assets_name, assets_url, end_game_path)

            else:
                sys.stderr.write(f"Skipped {game_name}, as neither assets nor executable required an update\n")

        else: # No executable provided
            sys.stderr.write(f"Warning: game {game_name} does not have an executable, so not unpacking assets either. Skipping.\n")



def check_updates(fd, destination_system, destination_architecture, destination_path, allow_downgrading):
    file_contents = {}
    try:
        json_contents = json.load(fd)
        if type(json_contents) != dict: 
            raise json.JSONDecodeError("Invalid content in the updater info file. Expected a JSON object.", doc=str(json_contents), pos=0)
        file_contents = json_contents
    except json.JSONDecodeError as e:
        # Invalid content, treat it as if it isn't there 
        sys.stderr.write(f"Warning: invalid content in the updater info file. Treating it as if it isn't there. File path: {updater_info_path.__str__()}\n")
        # Clearing and initializing the file
        clear_file(fd)
        fd.write("{}")



    response = requests.get("https://api.github.com/repos/" + GITHUB_REPO + "/releases/latest");
    release_info = response.json()


    # destination path is a file. We don't want to remove it unless required.
    if os.path.isfile(destination_path):
        if allow_removing_destination_path:
            try:
                os.unlink(destination_path)
            except PermissionError as e:
                sys.stderr.write("Attempted to remove the file, but did not have permissions. Refusing to continue.\n")
                exit(6)
        else:
            sys.stderr.write("The destination path is a file. Refusing to continue.\n")
            exit(5)

    try:
        # Compare the version of the last release with the version of currently installed app. 
        remote_version = version.parse(release_info["tag_name"])
        # get the current version, default to "v0"
        local_version = version.parse(file_contents.get("last_checked_release_id", "v0"))
        if local_version > remote_version:
            if allow_downgrading:
                sys.stderr.write("Warning: the version of currently installed app is higher than the version of last release.\n")
            else:
                sys.stderr.write("Error: the version of currently installed app is higher than the version of last release. No downgrading allowed. Local version: " + str(local_version) + ", remote version: " + str(remote_version) + "\n")
                sys.stderr.write("Error: downgrading may be allowed with --allow-downgrade\n")
                exit(3)
        elif local_version == remote_version:
            sys.stderr.write("The version of currently installed app is the same as the version of last release. No update needed.\n")
            exit(0)

        available_games = defaultdict(dict)

        file_contents["last_checked_release_id"] = release_info["tag_name"]
        if file_contents.get("files") is None:
            file_contents["files"] = {}

        # We got here means we take the new release
        for asset in release_info["assets"]:
            if not file_readable_to_updater(asset["name"]):
                sys.stderr.write(f"Warning: updater does not work with files of the type of {asset["name"]}. Skipping it.\n")
                continue
            file_name = asset["name"].split(".")[0] or ""
            filename_segments = file_name.split("-") or [None]
            game_name = filename_segments[0]
            executable_os = filename_segments[1] if len(filename_segments) > 1 else ""
            architecture = filename_segments[2] if len(filename_segments) > 2 else ""
            if executable_os == 'assets': 
                available_games[game_name]['assets'] =  {
                    'url': asset['browser_download_url'],
                    'hash': (asset['digest'])[len('sha256:'):],
                    'name': asset['name']
                }
            # Only look for the system that we are on
            elif executable_os == destination_system:
                if architecture is None:
                    sys.stderr.write("Warning: no architecture specified for the asset " + asset["name"] + ". Skipping it.\n")
                    continue
                elif architecture in ['x86', 'arm', 'x64','arm64']:
                    available_games[game_name][architecture] = {
                        'url': asset['browser_download_url'],
                        'hash': (asset['digest'])[len('sha256:'):],
                        'name': asset['name']
                    }
                else:
                    sys.stderr.write("Warning: unsupported architecture " + architecture + " for the asset " + asset["name"] + ". Skipping it.\n")
                    continue
            

        # Create the directory for games if it does not exist yet
        if not os.path.isdir(destination_path):
            try:
                Path(destination_path).mkdir(parents=True)
            except PermissionError:
                sys.stderr.write(f"Could not proceed with creating directory in {destination_path}. Refusing to proceed.\n")
                exit(6)
        # Unpack available games and assets for the given platform
        unpack_games(available_games, file_contents, destination_path, destination_architecture)

        clear_file(fd)
        json.dump(file_contents, fd)
    except KeyError as e:
        sys.stderr.write(f"Error: could not get last release from the GH Releases. Please ensure that repository https://github.com/{GITHUB_REPO}/ exists and has at least one current release.\n")
        exit(11)
try:
    # TODO: read argument from the input
    dest_platform = profile_platform(sys.platform)
    if dest_platform is None:
        sys.stderr.write("Error: unsupported platform. Please use one of linux, win, macos\n")
        exit(12)

    dest_arch = profile_architecture(platform.machine())
    if dest_arch == None:
        sys.stderr.write("Error: unsupported platform. Please use one of arm, arm64, x86, x64\n")
        exit(8)

    for arg in sys.argv:
        if arg == "--allow-downgrade":
            allow_downgrading = True
            sys.stderr.write("Warning: Downgrading allowed by the command.\n")
            continue

        dest_path_prefix = '--destination-path='
        if arg.startswith(dest_path_prefix):
            maybe_dest_path = arg[len(dest_path_prefix):]
            if len(maybe_dest_path) == 0:
                sys.stderr.write("Error: destination path has not been entered\n")
                exit(9)
            dest_path = maybe_dest_path

        updater_info_file_prefix = '--updater-info-file='
        if arg.startswith(updater_info_file_prefix):
            maybe_updater_info_file = arg[len(updater_info_file_prefix):]
            if len(maybe_updater_info_file) == 0:
                sys.stderr.write("Error: updater info file path has not been entered\n")
                exit(10)
            updater_info_path = maybe_updater_info_file

    updater_info_path = Path(dirname(__file__)).joinpath(updater_info_path).resolve()
    dest_path = Path(dirname(__file__)).joinpath(dest_path).resolve()

    f = None
    try:
        if not os.path.isfile(updater_info_path):
            Path(updater_info_path).touch()
        with open(updater_info_path, "r+") as f:
            check_updates(f, destination_system=dest_platform, destination_architecture=dest_arch, destination_path=dest_path, allow_downgrading=allow_downgrading)
    except PermissionError as e:
        sys.stderr.write("Error: no permission to write to the updater info file. File path: " + updater_info_path + "\n")
        exit(2)
    except KeyboardInterrupt as e:
        sys.stderr.write('Interrupting... ')
        try:
            # close the file in case of being here
            f.__exit()
        except Exception:
            pass
        exit(99)

    exit(0)
except KeyboardInterrupt as e:
    sys.stderr.write('Interrupting... Some files may not have been closed properly')
    exit(99)
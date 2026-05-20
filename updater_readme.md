# Updater for the games for Arcade Games

In T1 2026, we decided to move to auto-update from GitHub Releases sections, rather than just pulling the entire repository and hoping that the games are compiled for everything in there. For this, two conditions have to be met:
* The Arcade Games repository should consistently update the Releases section, including changing the version (tag) of the releases
* The Arcade Games repository's contents should follow the file structure (otherwise the updater will not be able to correctly unpack the games)

## Instructions on use of updater
The updater script has to be called via `python3 ${PATH_TO_UPDATER}` (or `skm python3 ${PATH_TO_UPDATER}`, in case if we are more assured that it is installed). The command accepts the following arguments:
* `--allow-downgrade` - allows downgrading to lower versions in the repository (i.e. the maintainer released `v2` but then made `v1` the latest), without this option, the updater will not proceed in this case
* `--destination-path=${DESTINATION_PATH}`, where `${DESTINATION_PATH}` is the path to the directory into which the updater will unpack the games. Default: `$(pwd)/games`
* `--updater-info-file=${UPDATER_INFO_FILE_PATH}`, where `${UPDATER_INFO_FILE_PATH}` is the path to the file that updater will use to store the info about its previous and current actions. Default: `${pwd}/updater.json` The info stored there is:
    * the tag of the last pulled release
    * the hash sums of the previously pulled files (so the updater does not try to pull the games from new release if neither of the files has been updated)

## Instructions on packaging the releases
The updater analyses the files in the Release by filtering the names of the files. It is expected that every eligible file there is a `.tar.gz` file (as the updater opens them with Python's built-in tar module).
The naming has to match the following template:
* Every archive with executable should be named `${game_name}-${os}-${architecture}.tar.gz`, and every asset archive `${game_name}-assets.tar.gz`.
* The updater will unpack the contents into `${DESTINATION_PATH}/${game_name}` exactly as they are in the archive, hence:
    * asset archive should have `Resources` folder (in most games) and `config.txt` (otherwise, `arcade-machine` will not read it) on the top level of the archive
    * executable archive should have `builds` folder on top level and it should contain the executable that the `arcade-machine` will be looking on a given system

Any files that do not end with `.tar.gz` will be ignored by the script.
# tempsystem
- Quickly create a temporary system (docker container) for testing purposes.
- Container based on `archlinux:latest`.
- Difference between just using `archlinux:latest` with Docker CLI: Includes sane defaults, pretty terminal theme/plugins, etc.

## Usage
### Downloading
1. Go to latest build action at https://codeberg.org/Land/tempsystem/actions.
2. Download the `linux.zip` artifact.
3. The binary is contained in the downloaded zip.

### Building
```sh
# Install deps
# Arch Linux
pacman -S make git curl gcc cmake jsoncpp

# Clone the repo
git clone https://codeberg.org/Land/tempsystem.git
cd tempsystem

# Build
make build
```

### Options
| **Argument**            | **Description**                                                                                   |
|----------------------------|---------------------------------------------------------------------------------------------------|
| `-v, --verbose`             | Increases output verbosity.                                                                      |
| `-r, --ro-root`             | Mounts the system root as read-only (cannot be used with `--extra-packages`).                    |
| `-c, --ro-cwd`              | Mounts the current directory as read-only.                                                       |
| `-m, --disable-cwd-mount`   | Prevents mounting the current directory to `~/work`.                                            |
| `-n, --no-network`          | Disables network capabilities for the system (cannot be used with `--extra-packages`).            |
| `-p, --extra-packages`      | Specifies extra packages to install in the system, space-delimited (cannot be used with `--no-network` or `--ro-root`). |
| `-ap, --extra-aur-packages` | same as --extra-packages, but fetches the packages from the AUR. |
#### Example
```sh
tempsystem --extra-packages "nodejs"
LOG: Pulling codeberg.org/land/tempsystem:latest...
LOG: Creating temporary system...
LOG: Starting system...
LOG: Installing package nodejs
LOG: Entering...
tempsystem@tempsystem ~/work (master*) $ node --version
v23.3.0
tempsystem@tempsystem ~/work (master*) $ 
```
#!/bin/sh

NAME="$(basename "$0")"

run() {
	echo [$NAME] $@
	$@
}

setup() {
	run sudo pacman --needed --noconfirm -Sy zsh curl git figlet lolcat fzf openssl sudo
	run curl "https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh" --location --retry-connrefused --retry 10 --fail -s -o /tmp/ohmyzsh-install.sh

	run chmod +x /tmp/ohmyzsh-install.sh

	run /tmp/ohmyzsh-install.sh --unattended --keep-zshrc
	run rm -f /tmp/ohmyzsh-install.sh

	run git clone https://github.com/zsh-users/zsh-autosuggestions ~/.oh-my-zsh/custom/plugins/zsh-autosuggestions
	run git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ~/.oh-my-zsh/custom/plugins/zsh-syntax-highlighting
}

setup
sudo rm /tmp/setup.sh
#!/bin/sh
# qgit-askpass - SSH/Git askpass helper for qgit
# Uses kdialog to prompt for passwords/passphrases.
# Set SSH_ASKPASS and GIT_ASKPASS to the path of this script.

PROMPT="${1:-Password:}"

exec kdialog --title "QGit - Authentication" --password "$PROMPT"

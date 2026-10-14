#!/bin/bash
# Run this from your desktop to launch ubmeshcore on the phone

adb shell "sudo -u phablet env \
  XDG_RUNTIME_DIR=/run/user/\$(adb shell id -u phablet | tr -d '\r') \
  QT_QPA_PLATFORM=ubuntumirclient \
  QT_QPA_PLATFORMTHEME=Lomiri \
  /opt/click.ubuntu.com/ubmeshcore/0.2.1/ubmeshcore 2>&1"

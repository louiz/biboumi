FROM docker.io/base/archlinux:latest

RUN pacman -Syuuuu --noconfirm

RUN pacman -Syu --noconfirm cmake base-devel git clang-tools-extra

RUN useradd -m -G wheel -s /bin/bash builder

RUN sed -i '/^# %wheel ALL=(ALL) NOPASSWD: ALL/s/^# //' /etc/sudoers

WORKDIR /home/builder

USER builder

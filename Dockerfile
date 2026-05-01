FROM devkitpro/devkitppc:latest

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=${DEVKITPRO}/devkitPPC
ENV LIBOGC=${DEVKITPRO}/libogc
ENV PATH=${DEVKITPPC}/bin:${DEVKITPRO}/tools/bin:${PATH}

RUN dkp-pacman -Syu --noconfirm \
    && dkp-pacman -S --noconfirm wii-dev

WORKDIR /workspace
CMD ["/bin/bash"]

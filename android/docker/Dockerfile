FROM ubuntu:24.04@sha256:2e863c44b718727c860746568e1d54afd13b2fa71b160f5cd9058fc436217b30

RUN apt-get update  \
    && apt-get install -y git openjdk-17-jdk unzip wget gcc g++  \
    && rm -rf /var/lib/apt/lists/*

ENV ANDROID_COMMAND_LINE_TOOLS_FILENAME commandlinetools-linux-10406996_latest.zip
ENV ANDROID_API_LEVELS                  android-34
ENV ANDROID_BUILD_TOOLS_VERSION         34.0.0

ENV ANDROID_HOME /usr/local/android-sdk-linux
ENV PATH         ${PATH}:${ANDROID_HOME}/tools:${ANDROID_HOME}/platform-tools:${ANDROID_HOME}/cmdline-tools/bin

RUN cd /usr/local/
RUN wget -q "https://dl.google.com/android/repository/${ANDROID_COMMAND_LINE_TOOLS_FILENAME}"
RUN unzip ${ANDROID_COMMAND_LINE_TOOLS_FILENAME} -d /usr/local/android-sdk-linux
RUN rm ${ANDROID_COMMAND_LINE_TOOLS_FILENAME}

RUN yes | sdkmanager --update --sdk_root="${ANDROID_HOME}"
RUN yes | sdkmanager --sdk_root="${ANDROID_HOME}" "platforms;${ANDROID_API_LEVELS}" "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" "extras;google;m2repository" "extras;android;m2repository" "extras;google;google_play_services" "ndk;26.3.11579264" "cmake;3.22.1"

RUN yes | sdkmanager --licenses --sdk_root="${ANDROID_HOME}"

RUN rm -rf ${ANDROID_HOME}/tools

RUN git config --global --add safe.directory /project

WORKDIR /build

COPY . .

WORKDIR /build/android

RUN ./gradlew assembleFdroidBeta -Parm64 && ./gradlew clean

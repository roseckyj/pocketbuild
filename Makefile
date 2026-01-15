IMAGE ?= pocketmake/build
APP_NAME ?= build
IP ?= 192.168.0.175

.PHONY: build setup send run

setup:
	docker build -t $(IMAGE) .

build:
	docker run --rm -it --mount type=bind,source="$$(pwd)",target=/project $(IMAGE)

send:
	./app-sender.sh build/build.app $(APP_NAME).app $(IP)

run: build send
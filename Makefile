IMAGE ?= pocketmake/build
IP ?=

.PHONY: setup build send

setup:
	docker build -t $(IMAGE) .

build:
	docker run --rm -it --mount type=bind,source="$$(pwd)",target=/project $(IMAGE)

send:
	./app-sender.sh build/demo build.app $(IP)

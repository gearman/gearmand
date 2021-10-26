IMAGE_NAME = gearmand
IMAGE_VERSION = 1.1.19.1
USE_CACHE ?=

image:
	@echo "Building Docker image ${IMAGE_NAME}:${IMAGE_VERSION}..."
	-docker build $(USE_CACHE) --network=host --build-arg version=$(IMAGE_VERSION) -t $(IMAGE_NAME):$(IMAGE_VERSION) .
	-(docker images -q -f dangling=true | xargs --no-run-if-empty docker rmi)

image-no-cache:
	$(MAKE) -e USE_CACHE=--no-cache

latest: image
	@echo "Tagging Docker image ${IMAGE_NAME}:${IMAGE_VERSION} with latest..."
	-docker tag `docker image ls --format '{{.ID}}' $(IMAGE_NAME):$(IMAGE_VERSION)` $(IMAGE_NAME):latest
	-(docker images -q -f dangling=true | xargs --no-run-if-empty docker rmi)

IMAGE_NAME = gearmand-testing
IMAGE_BASE = ubuntu
IMAGE_VERSION = 26.04
IMAGE_TAG = $(IMAGE_NAME)/${IMAGE_BASE}-clang22:$(IMAGE_VERSION)
USE_CACHE ?=

image:
	@echo "Building Docker image ${IMAGE_TAG}..."
	-docker build $(USE_CACHE) --network=host -t $(IMAGE_TAG) . && (docker images -q -f dangling=true | xargs --no-run-if-empty docker rmi)

image-no-cache:
	$(MAKE) -e USE_CACHE=--no-cache

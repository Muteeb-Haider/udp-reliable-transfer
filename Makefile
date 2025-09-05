.PHONY: build run-server run-client compose-up clean test

build:
	cmake -B build -S .
	cmake --build build --config Release

run-server:
	./build/bin/server --port 9000 --out ./server_data

run-client:
	./build/bin/client --host 127.0.0.1 --port 9000 --file ./sample_data/sample.bin

compose-up:
	docker compose up --remove-orphans

clean:
	rm -rf build server_data/* test_results/*

test: build
	python tests/robot/run_tests.py --install-requirements --output-dir ./test_results

test-basic: build
	python tests/robot/run_tests.py --tags basic --output-dir ./test_results

test-transfer: build
	python tests/robot/run_tests.py --tags transfer --output-dir ./test_results

test-error: build
	python tests/robot/run_tests.py --tags error --output-dir ./test_results

PORT ?= 7689
HOST ?= 127.0.0.1

.PHONY: serve clean

serve:
	@echo "serving args-parser diagram at http://$(HOST):$(PORT)"
	python3 -m http.server $(PORT) --bind $(HOST)

clean:
	@echo "nothing to clean (static site)"

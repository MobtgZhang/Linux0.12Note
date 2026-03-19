# Optional reference build — does not produce Kernel_Image.
# See docs/UEFI_X86_64_PORT.md (phases 1–3).

AS64 := $(shell command -v x86_64-linux-gnu-as 2>/dev/null || command -v as 2>/dev/null)

reference_long_mode.o: reference_long_mode.S
	@if [ -z "$(AS64)" ]; then echo "No assembler found"; exit 1; fi
	@if echo | $(AS64) --64 -v 2>&1 | grep -q "GNU assembler"; then \
		$(AS64) --64 -o $@ $<; \
	else \
		$(AS64) -o $@ $<; \
	fi

reference: reference_long_mode.o
	@echo "Built $@ (reference only)"

clean:
	rm -f reference_long_mode.o

.PHONY: reference clean

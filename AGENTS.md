# Q80 code style

- Write all user-visible CLI output, logs, help text, and errors in simple English. Documentation may use Traditional Chinese.
- Each Python module uses `logging.getLogger(__name__)`. The CLI configures logging once at entry through `q80.log.configure()`; modules do not install handlers.
- Use `q80.log.success(logger, ...)` for success. Use `logger.error(...)` or let CLI `report_error()` report failures. The shared format is `[module] ✅` or `[module] ❌`.
- Use `logger.info(...)` for progress and `logger.debug(...)` for details. These levels have no symbol; success and errors use `✅` and `❌`. Do not report the same failure twice.
- Keep raw shell output, native XCB output, file contents, and directory listings unchanged. Do not insert log prefixes into device data.
- Never log Wi-Fi passwords, command text, device serial numbers, or other secrets. Use fake data in formatting tests.

all: redis

redis: $(PHP_REDIS_OBJS) $(PHP_GLOBAL_OBJS)
	$(BUILD_REDIS)

install-redis: redis
	@echo "Installing PHP REDIS binary:        $(prefix)/bin/"
	@$(mkinstalldirs) $(prefix)/bin
	@$(INSTALL) -m 0755 redis $(prefix)/bin/$(program_prefix)asynclog-redis$(program_suffix)$(EXEEXT)

install: install-redis

clean-redis:
	rm -f redis

clean: clean-redis

# asynclog
PHP异步日志收集

### syslog调试信息的输出
```sh
phpize && CFLAGS=-DASYNCLOG_DEBUG ./configure && make install
tailf /var/log/syslog | grep asynclog
```

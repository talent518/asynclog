# asynclog
PHP异步日志收集

### syslog调试信息的输出
```sh
# 编译asynclog扩展
phpize && CFLAGS=-DASYNCLOG_DEBUG ./configure && make install

# rsyslog日志采集器配置
echo ':programname,isequal,"asynclog" /var/log/asynclog.log' > /etc/rsyslog.d/20-asynclog.conf
# 重启rsyslogd服务
sudo service rsyslog restart

# 监控调试日志
tailf /var/log/asynclog.log | sed 's|#012|\n|g'
```

### php.ini配置
```ini
; 日志存储方式: ASYNCLOG_MODE_FILE, ASYNCLOG_MODE_REDIS, ASYNCLOG_MODE_ELASTIC
asynclog.mode =           ASYNCLOG_MODE_FILE
; 日志级别: ASYNCLOG_LEVEL_ALL = PHP_ASYNCLOG_LEVEL_ERROR | PHP_ASYNCLOG_LEVEL_WARN | PHP_ASYNCLOG_LEVEL_INFO | PHP_ASYNCLOG_LEVEL_DEBUG | PHP_ASYNCLOG_LEVEL_VERBOSE
asynclog.level =          ASYNCLOG_LEVEL_ALL
; 文件路径
asynclog.filepath =     "/var/log/asynclog/"
; Redis主机名或IP
asynclog.redis_host =            "127.0.0.1"
; Redis端口号
asynclog.redis_port =                   6379
; Redis认证密码
asynclog.redis_auth =                     ""
; Elastic搜索引擎URL地址
asynclog.elastic =   "http://127.0.0.1:9200"
```

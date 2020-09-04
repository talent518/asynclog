--TEST--
asynclog('file', ASYNCLOG_LEVEL_INFO, 'tests/003.phpt')
--SKIPIF--
<?php if (!function_exists("asynclog")) print "skip"; ?>
--FILE--
<?php
echo asynclog('file', ASYNCLOG_LEVEL_INFO, 'tests/003.phpt'), PHP_EOL;
?>
--EXPECT--
name: file, category: application, level: 4, message: tests/003.phpt

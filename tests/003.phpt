--TEST--
asynclog('file', ASYNCLOG_LEVEL_INFO, 'tests/003.phpt')
--SKIPIF--
<?php if (!function_exists("asynclog")) print "skip"; ?>
--FILE--
<?php
echo asynclog('file', ASYNCLOG_LEVEL_INFO, 'tests/003.phpt'), PHP_EOL;
?>
--EXPECT--
[file][application][info] tests/003.phpt

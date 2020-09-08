--TEST--
asynclog('file', ASYNCLOG_LEVEL_INFO, 'tests/003.phpt')
--SKIPIF--
<?php if (!function_exists("asynclog")) print "skip"; ?>
--FILE--
<?php
var_export(asynclog('file', ASYNCLOG_LEVEL_INFO, 'tests/003.phpt'));
?>
--EXPECT--
true

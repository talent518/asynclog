--TEST--
asynclog() presence
--SKIPIF--
<?php if (!function_exists("asynclog")) print "skip"; ?>
--FILE--
<?php
echo "asynclog() is available";
?>
--EXPECT--
asynclog() is available

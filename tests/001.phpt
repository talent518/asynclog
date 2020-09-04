--TEST--
asynclog extension presence
--SKIPIF--
<?php if (!extension_loaded("asynclog")) print "skip"; ?>
--FILE--
<?php
echo "asynclog extension is available";
?>
--EXPECT--
asynclog extension is available

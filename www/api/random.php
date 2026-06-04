<?php
/* MajaRadio server-side random MOD endpoint. Plain text, Amiga-friendly. */
header('Content-Type: text/plain; charset=US-ASCII');
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');

$list_file = __DIR__ . '/list.txt';
$domain = 'mods.c64.social';

function fail_msg($msg) {
    echo "ERROR\n";
    echo "MESSAGE=" . $msg . "\n";
    exit;
}

$fh = @fopen($list_file, 'r');
if (!$fh) {
    fail_msg('list.txt not found');
}

$selected = null;
$count = 0;
while (($line = fgets($fh)) !== false) {
    $line = trim($line);
    if ($line === '' || $line[0] === '#') {
        continue;
    }
    $parts = explode('|', $line, 4);
    if (count($parts) !== 4) {
        continue;
    }
    $count++;
    if (mt_rand(1, $count) === 1) {
        $selected = $parts;
    }
}
fclose($fh);

if ($selected === null) {
    fail_msg('no MOD files indexed');
}

$id = $selected[0];
$size = $selected[1];
$path = $selected[2];
$title = $selected[3];
$url = (strpos($path, 'http://') === 0) ? $path : ('http://' . $domain . $path);

echo "OK\n";
echo "ID=" . $id . "\n";
echo "TITLE=" . $title . "\n";
echo "SIZE=" . $size . "\n";
echo "PATH=" . $path . "\n";
echo "URL=" . $url . "\n";

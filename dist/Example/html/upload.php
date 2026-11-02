<!--
/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2020  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
-->
<html>
<head><style>
    body {
        font-family: monospace;
        font-size: 12px;
    }
</style></head>
<body>

<?php
function h($v)
{
    return htmlspecialchars((string) $v, ENT_QUOTES, 'UTF-8');
}

function uploadTmpDir()
{
    $tmp = sys_get_temp_dir();
    return ($tmp !== '' && $tmp !== false) ? rtrim($tmp, "/\\") : '/tmp';
}

function underTmp($path)
{
    if (!is_string($path) || $path === '' || strpos($path, "\0") !== false) {
        return false;
    }
    if (preg_match('#^[a-zA-Z][a-zA-Z0-9+.\-]*://#', $path)) {
        return false;
    }
    $real = realpath($path);
    if ($real === false || !is_file($real)) {
        return false;
    }
    $tmp = uploadTmpDir();
    return strncmp($real, $tmp . DIRECTORY_SEPARATOR, strlen($tmp) + 1) === 0;
}

function moveAndShow($src, $destName, $trusted = false)
{
    if (empty($src)) {
        echo "<p>file is empty, not stored.</p>\n";
        return;
    }
    if (!$trusted && !underTmp($src)) {
        echo "<p>invalid source, not stored.</p>\n";
        return;
    }
    $dest = uploadTmpDir() . DIRECTORY_SEPARATOR . basename((string) $destName);
    $fileType = strtolower(pathinfo($dest, PATHINFO_EXTENSION));
    $check = @getimagesize($src);
    if ($check == false) {
        echo "<p>file is not jpg file, not stored.</p>\n";
    } else {
        if ($fileType != "jpg") {
            echo "<p>file type is not jpg, not stored.</p>\n";
        } else {
            rename($src, $dest);
            echo "<p>Moved: " . h($src) . " ====> " . h($dest) . "<br>";
            echo "MD5  : " . h(md5_file($dest)) . "<br>";
            echo "Size : " . h(filesize($dest)) . " bytes</p>\n";
        }
    }
}

function disaplyParsedFile($filekey)
{
    echo "<p>File : " . h($filekey) . "<br>";
    echo "Name : " . h($_POST["{$filekey}_name"] ?? '') . "<br>";
    echo "Type : " . h($_POST["{$filekey}_content-type"] ?? '') . "<br>";
    echo "Path : " . h($_POST["{$filekey}_path"] ?? '') . "<br>";
    echo "MD5  : " . h($_POST["{$filekey}_md5"] ?? '') . "<br>";
    echo "Size : " . h($_POST["{$filekey}_size"] ?? '') .  " Bytes<br></p>\n";
}

function displayNoParsedFile($filekey)
{
    echo "<p>File : " . h($filekey) . "<br>";
    echo "Name : " . h($_FILES["{$filekey}"]['name']) . "<br>";
    echo "Type : " . h($_FILES["{$filekey}"]['type']) . "<br>";
    echo "Path : " . h($_FILES["{$filekey}"]['tmp_name']) . "<br>";
    echo "Size : " . h($_FILES["{$filekey}"]['size']) . "</p>\n";
}




if(empty($_FILES["file1"]))
{
    echo "<h1>Request body updated by Parser</h1>\n";

    for ($i = 1; $i <= 2; $i++) {
        disaplyParsedFile("file{$i}");
        moveAndShow($_POST["file{$i}_path"] ?? '', $_POST["file{$i}_name"] ?? '');
    }

} else {
    echo "<h1>No Parser used</h1>\n";
    for ($i = 1; $i <= 2; $i++) {
        displayNoParsedFile("file{$i}");
        $tmpName = $_FILES["file{$i}"]["tmp_name"];
        $src = is_uploaded_file($tmpName) ? $tmpName : '';
        moveAndShow($src, $_FILES["file{$i}"]["name"], $src !== '');
    }
}
?>

</body></html>

<?php
// power.php
if ($_POST['action'] == 'reset') {
    $url = 'http://10.5.5.6:5000/hard-reset'; // Proxmox IP + Python Port
    $token = 'MySecretKey123'; // Must match Python script

    $options = [
        'http' => [
            'header'  => "Authorization: $token\r\n",
            'method'  => 'POST',
            'timeout' => 5 // Don't hang forever if Proxmox is dead
        ],
    ];

    $context = stream_context_create($options);
    $result = @file_get_contents($url, false, $context);

    if ($result === FALSE) {
        http_response_code(500);
        echo "Failed to connect to Proxmox Listener.";
    } else {
        echo $result;
    }
}
?>

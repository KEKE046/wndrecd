workspace.clientActivated.connect(function(client) {
    if(client.caption) {
        callDBus('wrecd.Daemon.Daemon', '/wrecd/Daemon/Daemon', 'wrecd.Daemon', 'submit', client.caption);
    }
})
import sequences

from functions import send_stanza

scenario = (
    sequences.handshake(),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection(),
    )

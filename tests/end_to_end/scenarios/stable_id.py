from scenarios import *

import scenarios.simple_channel_join

# see https://xmpp.org/extensions/xep-0359.html

scenario = (
    scenarios.simple_channel_join.scenario,

    send_stanza("""<message id='first_id' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'>
      <origin-id xmlns='urn:xmpp:sid:0' id='client-origin-id-should-be-kept'/>
      <stanza-id xmlns='urn:xmpp:sid:0' id='client-stanza-id-should-be-removed' by='#foo%{irc_server_one}'/>
      <stanza-id xmlns='urn:xmpp:sid:0' id='client-stanza-id-should-be-kept' by='someother@jid'/>
      <body>coucou</body></message>"""),

    # Entities, which are routing stanzas, SHOULD NOT strip any elements
    # qualified by the 'urn:xmpp:sid:0' namespace from message stanzas
    # unless the preceding rule applied to those elements.
    expect_stanza("/message/stable_id:origin-id[@id='client-origin-id-should-be-kept']",
    # Stanza ID generating entities, which encounter a <stanza-id/>
    # element where the 'by' attribute matches the 'by' attribute they
    # would otherwise set, MUST delete that element even if they are not
    # adding their own stanza ID.
                  "/message/stable_id:stanza-id[@id][@by='#foo%{irc_server_one}']",
                  "!/message/stable_id:stanza-id[@id='client-stanza-id-should-be-removed']",
    # Entities, which are routing stanzas, SHOULD NOT strip
    # any elements qualified by the 'urn:xmpp:sid:0'
    # namespace from message stanzas unless the preceding
    # rule applied to those elements.
                  "/message/stable_id:stanza-id[@id='client-stanza-id-should-be-kept'][@by='someother@jid']",
    ),
)

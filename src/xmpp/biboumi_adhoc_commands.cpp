#include <xmpp/biboumi_adhoc_commands.hpp>
#include <xmpp/biboumi_component.hpp>
#include <utils/scopeguard.hpp>
#include <bridge/bridge.hpp>
#include <config/config.hpp>
#include <utils/string.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <biboumi.h>

#ifdef USE_DATABASE
#include <database/database.hpp>
#endif

#ifndef HAS_PUT_TIME
#include <ctime>
#endif

using namespace std::string_literals;

void DisconnectUserStep1(XmppComponent& xmpp_component, AdhocSession&, XmlNode& command_node)
{
  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);

  XmlSubNode x(command_node, "jabber:x:data:x");
  x["type"] = "form";
  XmlSubNode title(x, "title");
  title.set_inner("Disconnect a user from the gateway");
  XmlSubNode instructions(x, "instructions");
  instructions.set_inner("Choose a user JID and a quit message");
  XmlSubNode jids_field(x, "field");
  jids_field["var"] = "jids";
  jids_field["type"] = "list-multi";
  jids_field["label"] = "The JIDs to disconnect";
  XmlSubNode required(jids_field, "required");
  for (Bridge* bridge: biboumi_component.get_bridges())
    {
      XmlSubNode option(jids_field, "option");
      option["label"] = bridge->get_jid();
      XmlSubNode value(option, "value");
      value.set_inner(bridge->get_jid());
    }

  XmlSubNode message_field(x, "field");
  message_field["var"] = "quit-message";
  message_field["type"] = "text-single";
  message_field["label"] = "Quit message";
  XmlSubNode message_value(message_field, "value");
  message_value.set_inner("Disconnected by admin");
}

void DisconnectUserStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);

  // Find out if the jids, and the quit message are provided in the form.
  std::string quit_message;
  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const XmlNode* message_field = nullptr;
      const XmlNode* jids_field = nullptr;
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "jids")
          jids_field = field;
        else if (field->get_tag("var") == "quit-message")
          message_field = field;
      if (message_field)
        {
          const XmlNode* value = message_field->get_child("value", "jabber:x:data");
          if (value)
            quit_message = value->get_inner();
        }
      if (jids_field)
        {
          std::size_t num = 0;
          for (const XmlNode* value: jids_field->get_children("value", "jabber:x:data"))
            {
              Bridge* bridge = biboumi_component.find_user_bridge(value->get_inner());
              if (bridge)
                {
                  bridge->shutdown(quit_message);
                  num++;
                }
            }
          command_node.delete_all_children();

          XmlSubNode note(command_node, "note");
          note["type"] = "info";
          if (num == 0)
            note.set_inner("No user were disconnected.");
          else if (num == 1)
            note.set_inner("1 user has been disconnected.");
          else
            note.set_inner(std::to_string(num) + " users have been disconnected.");
          return;
        }
    }
  XmlSubNode error(command_node, ADHOC_NS":error");
  error["type"] = "modify";
  XmlSubNode condition(error, STANZA_NS":bad-request");
  session.terminate();
}

#ifdef USE_DATABASE

void ConfigureGlobalStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());

  auto options = Database::get_global_options(owner.bare());

  XmlSubNode x(command_node, "jabber:x:data:x");
  x["type"] = "form";
  XmlSubNode title(x, "title");
  title.set_inner("Configure some global default settings.");
  XmlSubNode instructions(x, "instructions");
  instructions.set_inner("Edit the form, to configure your global settings for the component.");

  XmlSubNode max_histo_length(x, "field");
  max_histo_length["var"] = "max_history_length";
  max_histo_length["type"] = "text-single";
  max_histo_length["label"] = "Max history length";
  max_histo_length["desc"] = "The maximum number of lines in the history that the server sends when joining a channel";

  {
    XmlSubNode value(max_histo_length, "value");
    value.set_inner(std::to_string(options.col<Database::MaxHistoryLength>()));
  }

  XmlSubNode record_history(x, "field");
  record_history["var"] = "record_history";
  record_history["type"] = "boolean";
  record_history["label"] = "Record history";
  record_history["desc"] = "Whether to save the messages into the database, or not";

  {
    XmlSubNode value(record_history, "value");
    value.set_name("value");
    if (options.col<Database::RecordHistory>())
      value.set_inner("true");
    else
      value.set_inner("false");
  }
}

void ConfigureGlobalStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);

  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const Jid owner(session.get_owner_jid());
      auto options = Database::get_global_options(owner.bare());
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          const XmlNode* value = field->get_child("value", "jabber:x:data");

          if (field->get_tag("var") == "max_history_length" &&
              value && !value->get_inner().empty())
            options.col<Database::MaxHistoryLength>() = atoi(value->get_inner().data());
          else if (field->get_tag("var") == "record_history" &&
                   value && !value->get_inner().empty())
            {
              options.col<Database::RecordHistory>() = to_bool(value->get_inner());
              Bridge* bridge = biboumi_component.find_user_bridge(owner.bare());
              if (bridge)
                bridge->set_record_history(options.col<Database::RecordHistory>());
            }
        }

      options.save(Database::db);

      command_node.delete_all_children();
      XmlSubNode note(command_node, "note");
      note["type"] = "info";
      note.set_inner("Configuration successfully applied.");
      return;
    }
  XmlSubNode error(command_node, ADHOC_NS":error");
  error["type"] = "modify";
  XmlSubNode condition(error, STANZA_NS":bad-request");
  session.terminate();
}

void ConfigureIrcServerStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());
  std::string server_domain;
  if ((server_domain = Config::get("fixed_irc_server", "")).empty())
    server_domain = target.local;
  auto options = Database::get_irc_server_options(owner.local + "@" + owner.domain,
                                                  server_domain);

  XmlSubNode x(command_node, "jabber:x:data:x");
  x["type"] = "form";
  XmlSubNode title(x, "title");
  title.set_inner("Configure the IRC server "s + server_domain);
  XmlSubNode instructions(x, "instructions");
  instructions.set_inner("Edit the form, to configure the settings of the IRC server "s + server_domain);

  XmlSubNode ports(x, "field");
  ports["var"] = "ports";
  ports["type"] = "text-multi";
  ports["label"] = "Ports";
  ports["desc"] = "List of ports to try, without TLS. Defaults: 6667.";
  for (const auto& val: utils::split(options.col<Database::Ports>(), ';', false))
    {
      XmlSubNode ports_value(ports, "value");
      ports_value.set_inner(val);
    }

#ifdef BOTAN_FOUND
  XmlSubNode tls_ports(x, "field");
  tls_ports["var"] = "tls_ports";
  tls_ports["type"] = "text-multi";
  tls_ports["label"] = "TLS ports";
  tls_ports["desc"] = "List of ports to try, with TLS. Defaults: 6697, 6670.";
  for (const auto& val: utils::split(options.col<Database::TlsPorts>(), ';', false))
    {
      XmlSubNode tls_ports_value(tls_ports, "value");
      tls_ports_value.set_inner(val);
    }

  XmlSubNode verify_cert(x, "field");
  verify_cert["var"] = "verify_cert";
  verify_cert["type"] = "boolean";
  verify_cert["label"] = "Verify certificate";
  verify_cert["desc"] = "Whether or not to abort the connection if the server’s TLS certificate is invalid";
  XmlSubNode verify_cert_value(verify_cert, "value");
  if (options.col<Database::VerifyCert>())
    verify_cert_value.set_inner("true");
  else
    verify_cert_value.set_inner("false");

  XmlSubNode fingerprint(x, "field");
  fingerprint["var"] = "fingerprint";
  fingerprint["type"] = "text-single";
  fingerprint["label"] = "SHA-1 fingerprint of the TLS certificate to trust.";
  if (!options.col<Database::TrustedFingerprint>().empty())
    {
      XmlSubNode fingerprint_value(fingerprint, "value");
      fingerprint_value.set_inner(options.col<Database::TrustedFingerprint>());
    }
#endif

  XmlSubNode pass(x, "field");
  pass["var"] = "pass";
  pass["type"] = "text-private";
  pass["label"] = "Server password";
  pass["desc"] = "Will be used in a PASS command when connecting";
  if (!options.col<Database::Pass>().empty())
    {
      XmlSubNode pass_value(pass, "value");
      pass_value.set_inner(options.col<Database::Pass>());
    }

  XmlSubNode after_cnt_cmd(x, "field");
  after_cnt_cmd["var"] = "after_connect_command";
  after_cnt_cmd["type"] = "text-single";
  after_cnt_cmd["desc"] = "Custom IRC command sent after the connection is established with the server.";
  after_cnt_cmd["label"] = "After-connection IRC command";
  if (!options.col<Database::AfterConnectionCommand>().empty())
    {
      XmlSubNode after_cnt_cmd_value(after_cnt_cmd, "value");
      after_cnt_cmd_value.set_inner(options.col<Database::AfterConnectionCommand>());
    }

  if (Config::get("realname_customization", "true") == "true")
    {
      XmlSubNode username(x, "field");
      username["var"] = "username";
      username["type"] = "text-single";
      username["label"] = "Username";
      if (!options.col<Database::Username>().empty())
        {
          XmlSubNode username_value(username, "value");
          username_value.set_inner(options.col<Database::Username>());
        }

      XmlSubNode realname(x, "field");
      realname["var"] = "realname";
      realname["type"] = "text-single";
      realname["label"] = "Realname";
      if (!options.col<Database::Realname>().empty())
        {
          XmlSubNode realname_value(realname, "value");
          realname_value.set_inner(options.col<Database::Realname>());
        }
    }

  XmlSubNode encoding_out(x, "field");
  encoding_out["var"] = "encoding_out";
  encoding_out["type"] = "text-single";
  encoding_out["desc"] = "The encoding used when sending messages to the IRC server.";
  encoding_out["label"] = "Out encoding";
  if (!options.col<Database::EncodingOut>().empty())
    {
      XmlSubNode encoding_out_value(encoding_out, "value");
      encoding_out_value.set_inner(options.col<Database::EncodingOut>());
    }

  XmlSubNode encoding_in(x, "field");
  encoding_in["var"] = "encoding_in";
  encoding_in["type"] = "text-single";
  encoding_in["desc"] = "The encoding used to decode message received from the IRC server.";
  encoding_in["label"] = "In encoding";
  if (!options.col<Database::EncodingIn>().empty())
    {
      XmlSubNode encoding_in_value(encoding_in, "value");
      encoding_in_value.set_inner(options.col<Database::EncodingIn>());
    }
}

void ConfigureIrcServerStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const XmlNode* x = command_node.get_child("x", "jabber:x:data");
  if (x)
    {
      const Jid owner(session.get_owner_jid());
      const Jid target(session.get_target_jid());
      std::string server_domain;
      if ((server_domain = Config::get("fixed_irc_server", "")).empty())
        server_domain = target.local;
      auto options = Database::get_irc_server_options(owner.local + "@" + owner.domain,
                                                      server_domain);
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          const XmlNode* value = field->get_child("value", "jabber:x:data");
          const std::vector<const XmlNode*> values = field->get_children("value", "jabber:x:data");
          if (field->get_tag("var") == "ports")
            {
              std::string ports;
              for (const auto& val: values)
                ports += val->get_inner() + ";";
              options.col<Database::Ports>() = ports;
            }

#ifdef BOTAN_FOUND
          else if (field->get_tag("var") == "tls_ports")
            {
              std::string ports;
              for (const auto& val: values)
                ports += val->get_inner() + ";";
              options.col<Database::TlsPorts>() = ports;
            }

          else if (field->get_tag("var") == "verify_cert" && value
                   && !value->get_inner().empty())
            {
              auto val = to_bool(value->get_inner());
              options.col<Database::VerifyCert>() = val;
            }

          else if (field->get_tag("var") == "fingerprint" && value &&
                   !value->get_inner().empty())
            {
              options.col<Database::TrustedFingerprint>() = value->get_inner();
            }

#endif // BOTAN_FOUND

          else if (field->get_tag("var") == "pass" &&
                   value && !value->get_inner().empty())
            options.col<Database::Pass>() = value->get_inner();

          else if (field->get_tag("var") == "after_connect_command" &&
                   value && !value->get_inner().empty())
            options.col<Database::AfterConnectionCommand>() = value->get_inner();

          else if (field->get_tag("var") == "username" &&
                   value && !value->get_inner().empty())
            {
              auto username = value->get_inner();
              // The username must not contain spaces
              std::replace(username.begin(), username.end(), ' ', '_');
              options.col<Database::Username>() = username;
            }

          else if (field->get_tag("var") == "realname" &&
                   value && !value->get_inner().empty())
            options.col<Database::Realname>() = value->get_inner();

          else if (field->get_tag("var") == "encoding_out" &&
                   value && !value->get_inner().empty())
            options.col<Database::EncodingOut>() = value->get_inner();

          else if (field->get_tag("var") == "encoding_in" &&
                   value && !value->get_inner().empty())
            options.col<Database::EncodingIn>() = value->get_inner();

        }

      options.save(Database::db);

      command_node.delete_all_children();
      XmlSubNode note(command_node, "note");
      note["type"] = "info";
      note.set_inner("Configuration successfully applied.");
      return;
    }
  XmlSubNode error(command_node, ADHOC_NS":error");
  error["type"] = "modify";
  XmlSubNode condition(error, STANZA_NS":bad-request");
  session.terminate();
}

void ConfigureIrcChannelStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());

  insert_irc_channel_configuration_form(command_node, owner, target);
}

void insert_irc_channel_configuration_form(XmlNode& node, const Jid& requester, const Jid& target)
{
  const Iid iid(target.local, {});

  auto options = Database::get_irc_channel_options_with_server_default(requester.local + "@" + requester.domain,
                                                                       iid.get_server(), iid.get_local());

  XmlSubNode x(node, "jabber:x:data:x");
  x["type"] = "form";
  XmlSubNode title(x, "title");
  title.set_inner("Configure the IRC channel "s + iid.get_local() + " on server "s + iid.get_server());
  XmlSubNode instructions(x, "instructions");
  instructions.set_inner("Edit the form, to configure the settings of the IRC channel "s + iid.get_local());

  XmlSubNode record_history(x, "field");
  record_history["var"] = "record_history";
  record_history["type"] = "list-single";
  record_history["label"] = "Record history for this channel";
  record_history["desc"] = "If unset, the value is the one configured globally";

  {
    // Value selected by default
    XmlSubNode value(record_history, "value");
    value.set_inner(options.col<Database::RecordHistoryOptional>().to_string());
  }
  // All three possible values
  for (const auto& val: {"unset", "true", "false"})
  {
    XmlSubNode option(record_history, "option");
    option["label"] = val;
    XmlSubNode value(option, "value");
    value.set_inner(val);
  }

  XmlSubNode encoding_out(x, "field");
  encoding_out["var"] = "encoding_out";
  encoding_out["type"] = "text-single";
  encoding_out["desc"] = "The encoding used when sending messages to the IRC server. Defaults to the server's “out encoding” if unset for the channel";
  encoding_out["label"] = "Out encoding";
  if (!options.col<Database::EncodingOut>().empty())
    {
      XmlSubNode encoding_out_value(encoding_out, "value");
      encoding_out_value.set_inner(options.col<Database::EncodingOut>());
    }

  XmlSubNode encoding_in(x, "field");
  encoding_in["var"] = "encoding_in";
  encoding_in["type"] = "text-single";
  encoding_in["desc"] = "The encoding used to decode message received from the IRC server. Defaults to the server's “in encoding” if unset for the channel";
  encoding_in["label"] = "In encoding";
  if (!options.col<Database::EncodingIn>().empty())
    {
      XmlSubNode encoding_in_value(encoding_in, "value");
      encoding_in_value.set_inner(options.col<Database::EncodingIn>());
    }

  XmlSubNode persistent(x, "field");
  persistent["var"] = "persistent";
  persistent["type"] = "boolean";
  persistent["desc"] = "If set to true, when all XMPP clients have left this channel, biboumi will stay idle in it, without sending a PART command.";
  persistent["label"] = "Persistent";
  {
    XmlSubNode value(persistent, "value");
    value.set_name("value");
    if (options.col<Database::Persistent>())
      value.set_inner("true");
    else
      value.set_inner("false");
  }
}

void ConfigureIrcChannelStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());

  if (handle_irc_channel_configuration_form(xmpp_component, command_node, owner, target))
    {
      command_node.delete_all_children();
      XmlSubNode note(command_node, "note");
      note["type"] = "info";
      note.set_inner("Configuration successfully applied.");
    }
  else
    {
      XmlSubNode error(command_node, ADHOC_NS":error");
      error["type"] = "modify";
      XmlSubNode condition(error, STANZA_NS":bad-request");
      session.terminate();
    }
}

bool handle_irc_channel_configuration_form(XmppComponent& xmpp_component, const XmlNode& node, const Jid& requester, const Jid& target)
{
  const XmlNode* x = node.get_child("x", "jabber:x:data");
  if (x)
    {
      if (x->get_tag("type") == "submit")
        {
          const Iid iid(target.local, {});
          auto options = Database::get_irc_channel_options(requester.bare(),
                                                           iid.get_server(), iid.get_local());
          for (const XmlNode *field: x->get_children("field", "jabber:x:data"))
            {
              const XmlNode *value = field->get_child("value", "jabber:x:data");

              if (field->get_tag("var") == "encoding_out" &&
                  value && !value->get_inner().empty())
                options.col<Database::EncodingOut>() = value->get_inner();

              else if (field->get_tag("var") == "encoding_in" &&
                       value && !value->get_inner().empty())
                options.col<Database::EncodingIn>() = value->get_inner();

              else if (field->get_tag("var") == "persistent" &&
                       value)
                options.col<Database::Persistent>() = to_bool(value->get_inner());
              else if (field->get_tag("var") == "record_history" &&
                       value && !value->get_inner().empty())
                {
                  OptionalBool& database_value = options.col<Database::RecordHistoryOptional>();
                  if (value->get_inner() == "true")
                    database_value.set_value(true);
                  else if (value->get_inner() == "false")
                    database_value.set_value(false);
                  else
                    database_value.unset();
                  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);
                  Bridge* bridge = biboumi_component.find_user_bridge(requester.bare());
                  if (bridge)
                    {
                      if (database_value.is_set)
                        bridge->set_record_history(database_value.value);
                      else
                        { // It is unset, we need to fetch the Global option, to
                          // know if it’s enabled or not
                          auto g_options = Database::get_global_options(requester.bare());
                          bridge->set_record_history(g_options.col<Database::RecordHistory>());
                        }
                    }
                }

            }

          options.save(Database::db);
        }
      return true;
    }
  return false;
}
#endif  // USE_DATABASE

void DisconnectUserFromServerStep1(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  const Jid owner(session.get_owner_jid());
  if (owner.bare() != Config::get("admin", ""))
    { // A non-admin is not allowed to disconnect other users, only
      // him/herself, so we just skip this step
      auto next_step = session.get_next_step();
      next_step(xmpp_component, session, command_node);
    }
  else
    { // Send a form to select the user to disconnect
      auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);

      XmlSubNode x(command_node, "jabber:x:data:x");
      x["type"] = "form";
      XmlSubNode title(x, "title");
      title.set_inner("Disconnect a user from selected IRC servers");
      XmlSubNode instructions(x, "instructions");
      instructions.set_inner("Choose a user JID");
      XmlSubNode jids_field(x, "field");
      jids_field["var"] = "jid";
      jids_field["type"] = "list-single";
      jids_field["label"] = "The JID to disconnect";
      XmlSubNode required(jids_field, "required");
      for (Bridge* bridge: biboumi_component.get_bridges())
        {
          XmlSubNode option(jids_field, "option");
          option["label"] = bridge->get_jid();
          XmlSubNode value(option, "value");
          value.set_inner(bridge->get_jid());
        }
    }
}

void DisconnectUserFromServerStep2(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  // If no JID is contained in the command node, it means we skipped the
  // previous stage, and the jid to disconnect is the executor's jid
  std::string jid_to_disconnect = session.get_owner_jid();

  if (const XmlNode* x = command_node.get_child("x", "jabber:x:data"))
    {
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        if (field->get_tag("var") == "jid")
          {
            if (const XmlNode* value = field->get_child("value", "jabber:x:data"))
              jid_to_disconnect = value->get_inner();
          }
    }

  // Save that JID for the last step
  session.vars["jid"] = jid_to_disconnect;

  // Send a data form to let the user choose which server to disconnect the
  // user from
  command_node.delete_all_children();
  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);

  XmlSubNode x(command_node, "jabber:x:data:x");
  x["type"] = "form";
  XmlSubNode title(x, "title");
  title.set_inner("Disconnect a user from selected IRC servers");
  XmlSubNode instructions(x, "instructions");
  instructions.set_inner("Choose one or more servers to disconnect this JID from");
  XmlSubNode jids_field(x, "field");
  jids_field["var"] = "irc-servers";
  jids_field["type"] = "list-multi";
  jids_field["label"] = "The servers to disconnect from";
  XmlSubNode required(jids_field, "required");
  Bridge* bridge = biboumi_component.find_user_bridge(jid_to_disconnect);

  if (!bridge || bridge->get_irc_clients().empty())
    {
      XmlSubNode note(command_node, "note");
      note["type"] = "info";
      note.set_inner("User "s + jid_to_disconnect + " is not connected to any IRC server.");
      session.terminate();
      return ;
    }

  for (const auto& pair: bridge->get_irc_clients())
    {
      XmlSubNode option(jids_field, "option");
      option["label"] = pair.first;
      XmlSubNode value(option, "value");
      value.set_inner(pair.first);
    }

  XmlSubNode message_field(x, "field");
  message_field["var"] = "quit-message";
  message_field["type"] = "text-single";
  message_field["label"] = "Quit message";
  XmlSubNode message_value(message_field, "value");
  message_value.set_inner("Killed by admin");
}

void DisconnectUserFromServerStep3(XmppComponent& xmpp_component, AdhocSession& session, XmlNode& command_node)
{
  const auto it = session.vars.find("jid");
  if (it == session.vars.end())
    return ;
  const auto jid_to_disconnect = it->second;

  std::vector<std::string> servers;
  std::string quit_message;

  if (const XmlNode* x = command_node.get_child("x", "jabber:x:data"))
    {
      for (const XmlNode* field: x->get_children("field", "jabber:x:data"))
        {
          if (field->get_tag("var") == "irc-servers")
            {
              for (const XmlNode* value: field->get_children("value", "jabber:x:data"))
                servers.push_back(value->get_inner());
            }
          else if (field->get_tag("var") == "quit-message")
            if (const XmlNode* value = field->get_child("value", "jabber:x:data"))
              quit_message = value->get_inner();
        }
    }

  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(xmpp_component);
  Bridge* bridge = biboumi_component.find_user_bridge(jid_to_disconnect);
  auto& clients = bridge->get_irc_clients();

  std::size_t number = 0;

  for (const auto& hostname: servers)
    {
      auto it = clients.find(hostname);
      if (it != clients.end())
        {
          it->second->on_error({"ERROR", {quit_message}});
          clients.erase(it);
          number++;
        }
    }
  command_node.delete_all_children();
  XmlSubNode note(command_node, "note");
  note["type"] = "info";
  std::string msg = jid_to_disconnect + " was disconnected from " + std::to_string(number) + " IRC server";
  if (number > 1)
    msg += "s";
  msg += ".";
  note.set_inner(msg);
}

void GetIrcConnectionInfoStep1(XmppComponent& component, AdhocSession& session, XmlNode& command_node)
{
  auto& biboumi_component = dynamic_cast<BiboumiComponent&>(component);

  const Jid owner(session.get_owner_jid());
  const Jid target(session.get_target_jid());

  std::string message{};

  // As the function is exited, set the message in the response.
  utils::ScopeGuard sg([&message, &command_node]()
                       {
                         command_node.delete_all_children();
                         XmlSubNode note(command_node, "note");
                         note["type"] = "info";
                         note.set_inner(message);
                       });

  Bridge* bridge = biboumi_component.get_user_bridge(owner.bare());
  if (!bridge)
    {
      message = "You are not connected to anything.";
      return;
    }

  std::string hostname;
  if ((hostname = Config::get("fixed_irc_server", "")).empty())
    hostname = target.local;

  IrcClient* irc = bridge->find_irc_client(hostname);
  if (!irc || !irc->is_connected())
    {
      message = "You are not connected to the IRC server "s + hostname;
      return;
    }

  std::ostringstream ss;
  ss << "Connected to IRC server " << irc->get_hostname() << " on port " << irc->get_port();
  if (irc->is_using_tls())
    ss << " (using TLS)";
  const std::time_t now_c = std::chrono::system_clock::to_time_t(irc->connection_date);
#ifdef HAS_PUT_TIME
  ss << " since " << std::put_time(std::localtime(&now_c), "%F %T");
#else
  constexpr std::size_t timestamp_size{10 + 1 + 8 + 1};
  char buf[timestamp_size] = {};
  const auto res = std::strftime(buf, timestamp_size, "%F %T", std::localtime(&now_c));
  if (res > 0)
    ss << " since " << buf;
#endif
  ss << " (" << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - irc->connection_date).count() << " seconds ago).";

  for (const auto& it: bridge->resources_in_chan)
    {
      const auto& channel_key = it.first;
      const auto& irc_hostname = std::get<1>(channel_key);
      const auto& resources = it.second;

      if (irc_hostname == irc->get_hostname() && !resources.empty())
        {
          const auto& channel_name = std::get<0>(channel_key);
          ss << "\n" << channel_name << " from " << resources.size() << " resource" << (resources.size() > 1 ? "s": "") << ": ";
          for (const auto& resource: resources)
            ss << resource << " ";
        }
    }

  message = ss.str();
}

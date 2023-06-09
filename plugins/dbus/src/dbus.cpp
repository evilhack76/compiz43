/*
 * Copyright © 2006 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 *
 * Ported to Compiz 0.9 by:
 * Copyright (C) 2009 Sam Spilsbury <smspillaz@gmail.com>
 * Copyright (C) 2013 Sami Jaktholm <sampo_75@windowslive.com> (introspection)
 */

#include "dbus.h"

using namespace boost::placeholders;

COMPIZ_PLUGIN_20090315 (dbus, DbusPluginVTable);

CompOption::Vector emptyList;

const char*
IntrospectionResponse::finishAndGetXml ()
{
    xmlTextWriterEndDocument (xmlWriter);
    return reinterpret_cast <const char *> (xmlBuf->content);
}

void 
IntrospectionResponse::startInterface ()
{
    xmlTextWriterStartElement (xmlWriter, BAD_CAST "interface");
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "name",
				 BAD_CAST COMPIZ_DBUS_SERVICE_NAME);
}

void
IntrospectionResponse::endInterface ()
{
    xmlTextWriterEndElement (xmlWriter);
}

void
IntrospectionResponse::addArgument (const char *type,
				    const char *direction)
{
    xmlTextWriterStartElement (xmlWriter, BAD_CAST "arg");
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "type", BAD_CAST type);
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "direction",
				 BAD_CAST direction);
    xmlTextWriterEndElement (xmlWriter);
}

void
IntrospectionResponse::addMethod (const char *name,
				  int        nArgs,
				  ...)
{
    va_list var_args;
    const char *type, *direction;

    xmlTextWriterStartElement (xmlWriter, BAD_CAST "method");
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "name", BAD_CAST name);

    va_start (var_args, nArgs);
    while (nArgs)
    {
	type = va_arg (var_args, const char *);
	direction = va_arg (var_args, const char *);
	addArgument (type, direction);
	--nArgs;
    }
    va_end (var_args);

    xmlTextWriterEndElement (xmlWriter);
}

void
IntrospectionResponse::addSignal (const char *name,
				  int        nArgs,
				  ...)
{
    va_list var_args;
    const char *type;

    xmlTextWriterStartElement (xmlWriter, BAD_CAST "signal");
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "name", BAD_CAST name);

    va_start (var_args, nArgs);
    while (nArgs)
    {
	type = va_arg (var_args, const char *);
	addArgument (type, "out");
	--nArgs;
    }
    va_end (var_args);

    xmlTextWriterEndElement (xmlWriter);
}

void
IntrospectionResponse::addNode (const char *name)
{
    xmlTextWriterStartElement (xmlWriter, BAD_CAST "node");
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "name", BAD_CAST name);
    xmlTextWriterEndElement (xmlWriter);
}

IntrospectionResponse::IntrospectionResponse ()
{
    xmlBuf = xmlBufferCreate ();
    xmlWriter = xmlNewTextWriterMemory (xmlBuf, 0);

    /* Add introspection node. */
    xmlTextWriterStartElement (xmlWriter, BAD_CAST "node");

    xmlTextWriterStartElement (xmlWriter, BAD_CAST "interface");
    xmlTextWriterWriteAttribute (xmlWriter, BAD_CAST "name",
				 BAD_CAST "org.freedesktop.DBus.Introspectable");

    addMethod ("Introspect", 1, "s", "out");

    xmlTextWriterEndElement (xmlWriter);
}

IntrospectionResponse::~IntrospectionResponse ()
{
    if (xmlWriter)
	xmlFreeTextWriter (xmlWriter);

    if (xmlBuf)
	xmlBufferFree (xmlBuf);
}

#ifdef __cplusplus
/* A simple wrapper for dbus to Compiz 0.9 */
extern "C"
{
#endif
    static DBusHandlerResult dbusHandleMessage (DBusConnection *,
						DBusMessage *,
						void *);

    DBusHandlerResult dbusHandleMessage (DBusConnection *c,
					 DBusMessage *m,
					 void *v)
    {
	DBUS_SCREEN (screen);

	return ds->handleMessage (c, m, v);
    }

    static DBusObjectPathVTable dbusMessagesVTable = {
	NULL, dbusHandleMessage, /* handler function */
	NULL, NULL, NULL, NULL
    };
#ifdef __cplusplus
}
#endif

CompOption::Vector &
DbusScreen::getOptionsFromPath (const std::vector<CompString>& path)
{
    CompPlugin *p;

    p = CompPlugin::find (path[0].c_str ());

    if (!p)
    {
	return emptyList;
    }

    if (p->vTable->getOptions ().empty ())
	return emptyList;

    return p->vTable->getOptions ();
}

bool
DbusScreen::sendIntrospectResponse (DBusConnection        *connection,
				    DBusMessage           *message,
				    IntrospectionResponse &response)
{
    DBusMessage *reply = dbus_message_new_method_return (message);
    if (!reply)
	return false;

    DBusMessageIter args;
    dbus_message_iter_init_append (reply, &args);

    const char* responseXml = response.finishAndGetXml ();

    if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &responseXml))
	return false;

    if (!dbus_connection_send (connection, reply, NULL))
	return false;

    dbus_connection_flush (connection);
    dbus_message_unref (reply);

    return true;
}

/* introspection handlers */
bool
DbusScreen::handleRootIntrospectMessage (DBusConnection *connection,
					 DBusMessage    *message)
{

    IntrospectionResponse response;
    response.startInterface ();

#if GET_PLUGIN_METADATA_ENABLED
    response.addMethod (COMPIZ_DBUS_GET_PLUGIN_METADATA_MEMBER_NAME, 7,
			"s", "in", "s", "out", "s", "out", "s", "out",
			"b", "out", "as", "out", "as", "out");
#endif

    response.addSignal (COMPIZ_DBUS_PLUGINS_CHANGED_SIGNAL_NAME, 0);
    response.endInterface ();

    const CompPlugin::List &plugins = CompPlugin::getPlugins ();

    if (plugins.empty ())
	return false;

    foreach (CompPlugin* p, plugins)
    {
	if (p->vTable)
	    response.addNode (p->vTable->name ().c_str());
    }

    return sendIntrospectResponse (connection, message, response);

}

/* MULTIDPYERROR: only works with one or less displays present */
bool
DbusScreen::handlePluginIntrospectMessage (DBusConnection *connection,
				   	   DBusMessage    *message)
{
    char screenName[256];
    IntrospectionResponse response;

    snprintf (screenName, 256, "screen%d", screen->screenNum ());
    response.addNode (screenName);

    return sendIntrospectResponse (connection, message, response);

}

bool
DbusScreen::handleScreenIntrospectMessage (DBusConnection           *connection,
					   DBusMessage              *message,
					   std::vector<CompString>& path)
{
    IntrospectionResponse response;

    response.startInterface ();
    response.addMethod (COMPIZ_DBUS_LIST_MEMBER_NAME, 1, "as", "out");
    response.endInterface ();

    CompOption::Vector &options = getOptionsFromPath (path);
    if (!options.empty ())
    {
	foreach (CompOption &option, options)
	{
	    response.addNode (option.name ().c_str());
	}
    }

    return sendIntrospectResponse (connection, message, response);
}

bool
DbusScreen::handleOptionIntrospectMessage (DBusConnection           *connection,
					   DBusMessage              *message,
					   std::vector<CompString>& path)
{
#if GET_PLUGIN_METADATA_ENABLED
    bool metadataHandled = false;
#endif
    bool isList = false;
    char type[3];

    IntrospectionResponse response;
    CompOption::Type      restrictionType;

    CompOption::Vector &options = getOptionsFromPath (path);
    CompOption         *option  = CompOption::findOption (options, path[2]);

    response.startInterface ();

    if (option)
    {
        restrictionType = option->type ();
        if (restrictionType == CompOption::TypeList)
        {
	    restrictionType = option->value ().listType ();
	    isList = true;
        }

        switch (restrictionType)
        {
        case CompOption::TypeInt:
	    if (isList)
	        strcpy (type, "ai");
	    else
	        strcpy (type, "i");
#if GET_PLUGIN_METADATA_ENABLED
	    response.addMethod (COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
			        6, "s", "out", "s", "out",
			        "b", "out", "s", "out",
			        "i", "out", "i", "out");
	    metadataHandled = true;
#endif
	    break;
        case CompOption::TypeFloat:
	    if (isList)
	        strcpy (type, "ad");
	    else
	        strcpy (type, "d");
#if GET_PLUGIN_METADATA_ENABLED
	    response.addMethod (COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
			        7, "s", "out", "s", "out",
			        "b", "out", "s", "out",
			        "d", "out", "d", "out",
			        "d", "out");
	    metadataHandled = true;
#endif
	    break;
        case CompOption::TypeString:
	    if (isList)
	        strcpy (type, "as");
	    else
	        strcpy (type, "s");
#if GET_PLUGIN_METADATA_ENABLED
	    response.addMethod (COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
			        5, "s", "out", "s", "out",
			        "b", "out", "s", "out",
			        "as", "out");
	    metadataHandled = true;
#endif
	    break;
        case CompOption::TypeBool:
        case CompOption::TypeBell:
	    if (isList)
	        strcpy (type, "ab");
	    else
	        strcpy (type, "b");

	    break;
        case CompOption::TypeColor:
        case CompOption::TypeKey:
        case CompOption::TypeButton:
        case CompOption::TypeEdge:
        case CompOption::TypeMatch:
	    if (isList)
	        strcpy (type, "as");
	    else
	        strcpy (type, "s");
	    break;
        default:
	    break;
        }

        response.addMethod (COMPIZ_DBUS_GET_MEMBER_NAME, 1,
			    type, "out");
        response.addMethod (COMPIZ_DBUS_SET_MEMBER_NAME, 1,
			    type, "in");
        response.addSignal (COMPIZ_DBUS_CHANGED_SIGNAL_NAME, 1,
			    type, "out");
#if GET_PLUGIN_METADATA_ENABLED
        if (!metadataHandled)
	    response.addMethod (COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
			        4, "s", "out", "s", "out",
			        "b", "out", "s", "out");
#endif
    }

    response.endInterface ();

    return sendIntrospectResponse (connection, message, response);
}


/*
 * Activate can be used to trigger any existing action. Arguments
 * should be a pair of { string, bool|int32|double|string }.
 *
 * Example (rotate to face 1):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/rotate/allscreens/rotate_to	      \
 * org.freedesktop.compiz.activate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`     \
 * string:'face' int32:1
 *
 *
 * You can also call the terminate function
 *
 * Example unfold and refold cube:
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/cube/allscreens/unfold	      \
 * org.freedesktop.compiz.activate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/cube/allscreens/unfold	      \
 * org.freedesktop.compiz.deactivate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`
 *
 */

bool
DbusScreen::handleActionMessage (DBusConnection                 *connection,
				 DBusMessage                    *message,
				 const std::vector<CompString>& path,
				 bool                           activate)
{
    CompOption::Vector &options = getOptionsFromPath (path);
    if (options.empty ())
	return false;

    foreach (CompOption& option, options)
    {
	if (option.name () == path[2])
	{
	    CompOption::Vector argument;
	    DBusMessageIter    iter;
	    CompAction         *action = &option.value ().action ();

	    if (option.type () != CompOption::TypeAction &&
		option.type () != CompOption::TypeKey    &&
		option.type () != CompOption::TypeButton &&
		option.type () != CompOption::TypeEdge   &&
		option.type () != CompOption::TypeBell)
		return false;

	    if (activate)
	    {
		if (!action->initiate ())
		    return false;
	    }
	    else
	    {
		if (!action->terminate ())
		    return false;
	    }

	    if (dbus_message_iter_init (message, &iter))
	    {
		CompOption::Value value;
		CompOption::Type  type;
		char		  *name;

		do
		{
		    name = NULL;
		    type = CompOption::TypeUnset;

		    while (!name)
		    {
			switch (dbus_message_iter_get_arg_type (&iter)) {
			case DBUS_TYPE_STRING:
			    dbus_message_iter_get_basic (&iter, &name);
			default:
			    break;
			}

			if (!dbus_message_iter_next (&iter))
			    break;
		    }

		    while (type == CompOption::TypeUnset)
		    {
			switch (dbus_message_iter_get_arg_type (&iter)) {
			case DBUS_TYPE_BOOLEAN:
			    {
				dbus_bool_t tmp;
				type = CompOption::TypeBool;
				dbus_message_iter_get_basic (&iter, &tmp);
				value.set (tmp ? true : false);
			    }
			    break;
			case DBUS_TYPE_INT32:
			    {
				int tmp;
				type = CompOption::TypeInt;
				dbus_message_iter_get_basic (&iter, &tmp);
				value.set (tmp);
			    }
			    break;
			case DBUS_TYPE_DOUBLE:
			    {
				double tmp;
				type = CompOption::TypeFloat;
				dbus_message_iter_get_basic (&iter, &tmp);
				value.set ((float) tmp);
			    }
			    break;
			case DBUS_TYPE_STRING:
			    {
				char *s;

				dbus_message_iter_get_basic (&iter, &s);

				/* XXX: use match option type if
				        name is "match" */
				if (name && strcmp (name, "match") == 0)
				{
				    type = CompOption::TypeMatch;
				    value.set (CompMatch (CompString (s)));
				}
				else
				{
				    type = CompOption::TypeString;
				    value.set (CompString (s));
				}
			    }
			default:
			    break;
			}

			if (!dbus_message_iter_next (&iter))
			    break;
		    }

		    if (name && type != CompOption::TypeUnset)
		    {
			CompOption arg (name, type);
			arg.set (value);
			argument.push_back (arg);
		    }
		} while (dbus_message_iter_has_next (&iter));
	    }

	    if (activate)
		action->initiate () (action, 0, argument);
	    else
		action->terminate () (action, 0, argument);

	    if (!dbus_message_get_no_reply (message))
	    {
		DBusMessage *reply;

		reply = dbus_message_new_method_return (message);

		dbus_connection_send (connection, reply, NULL);
		dbus_connection_flush (connection);

		dbus_message_unref (reply);
	    }

	    return true;
	}
    }

    return false;
}

bool
DbusScreen::tryGetValueWithType (DBusMessageIter *iter,
				 int		 type,
				 void		 *value)
{
    if (dbus_message_iter_get_arg_type (iter) == type)
    {
	dbus_message_iter_get_basic (iter, value);

	return true;
    }

    return false;
}

bool
DbusScreen::getOptionValue (DBusMessageIter   *iter,
			    CompOption::Type  type,
			    CompOption::Value &value)
{
    bool   success;

    switch (type) {
    case CompOption::TypeBool:
	{
	    dbus_bool_t b;
	    success = tryGetValueWithType (iter, DBUS_TYPE_BOOLEAN, &b);
	    if (success)
		value.set (b ? true : false);
	}
	break;
    case CompOption::TypeInt:
	{
	    int i;
	    success = tryGetValueWithType (iter, DBUS_TYPE_INT32, &i);
	    if (success)
		value.set (i);
	}
	break;
    case CompOption::TypeFloat:
	{
	    double d;
	    success = tryGetValueWithType (iter, DBUS_TYPE_DOUBLE, &d);
	    if (success)
		value.set ((float) d);
	}
	break;
    case CompOption::TypeString:
	{
	    char *s;
	    success = tryGetValueWithType (iter, DBUS_TYPE_STRING, &s);
	    if (success)
	        value.set (CompString (s));
	}
	break;
    case CompOption::TypeColor:
	{
	    char           *s;
	    unsigned short c[4];

	    success = tryGetValueWithType (iter, DBUS_TYPE_STRING, &s);
	    if (success)
		success &= CompOption::stringToColor (CompString (s), c);
	    if (success)
		value.set (c);
	}
	break;
    case CompOption::TypeKey:
	{
	    char       *s;
	    CompAction action;

	    success = tryGetValueWithType (iter, DBUS_TYPE_STRING, &s);
	    if (success)
		success = action.keyFromString (CompString (s));
	    if (success)
		value.set (action);
	}
	break;
    case CompOption::TypeButton:
	{
	    char       *s;
	    CompAction action;

	    success = tryGetValueWithType (iter, DBUS_TYPE_STRING, &s);
	    if (success)
		success = action.buttonFromString (CompString (s));
	    if (success)
		value.set (action);
	}
	break;
    case CompOption::TypeEdge:
	{
	    char       *s;
	    CompAction action;

	    success = tryGetValueWithType (iter, DBUS_TYPE_STRING, &s);
	    if (success)
		success = action.edgeMaskFromString (CompString (s));
	    if (success)
		value.set (action);
	}
	break;
    case CompOption::TypeBell:
	{
	    dbus_bool_t bell;
	    CompAction  action;

	    success = tryGetValueWithType (iter, DBUS_TYPE_BOOLEAN, &bell);
	    if (success)
	    {
		action.setBell (bell ? true : false);
		value.set (action);
	    }
	}
	break;
    case CompOption::TypeMatch:
	{
	    char *s;
	    success = tryGetValueWithType (iter, DBUS_TYPE_STRING, &s);
	    if (success)
		value.set (CompMatch (s));
	}
	break;
    default:
	success = false;
	break;
    }

    return success;
}

/*
 * 'Set' can be used to change any existing option. Argument
 * should be the new value for the option.
 *
 * Example (will set command0 option to firefox):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/core/allscreens/command0	      \
 * org.freedesktop.compiz.set				      \
 * string:'firefox'
 *
 * List and action options can be changed using more than one
 * argument.
 *
 * Example (will set active_plugins option to
 * [dbus,decoration,place]):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/core/allscreens/active_plugins     \
 * org.freedesktop.compiz.set				      \
 * array:string:'dbus','decoration','place'
 *
 * Example (will set run_command0 option to trigger on key
 * binding <Control><Alt>Return and not trigger on any button
 * bindings, screen edges or bell notifications):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/core/allscreens/run_command0	      \
 * org.freedesktop.compiz.set				      \
 * string:'<Control><Alt>Return'			      \
 * string:'Disabled'					      \
 * boolean:'false'					      \
 * string:''						      \
 * int32:'0'
 */
bool
DbusScreen::handleSetOptionMessage (DBusConnection                 *connection,
				    DBusMessage                    *message,
				    const std::vector<CompString>& path)
{
    CompOption::Vector &options = getOptionsFromPath (path);

    foreach (CompOption& option, options)
    {
	if (option.name () == path[2])
	{
	    DBusMessageIter   iter, aiter;
	    CompOption::Value value, tmpValue;
	    bool              status = false;

	    if (option.type () == CompOption::TypeList)
	    {
		if (dbus_message_iter_init (message, &iter) &&
		    dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_ARRAY)
		{
		    dbus_message_iter_recurse (&iter, &aiter);

		    do
		    {
			if (getOptionValue (&aiter,
					    option.value ().listType (),
					    tmpValue))
			{
			    option.value ().list ().push_back (tmpValue);
			}
		    } while (dbus_message_iter_next (&aiter));

		    status = true;
		}
	    }
	    else if (dbus_message_iter_init (message, &iter))
	    {
		status = getOptionValue (&iter, option.type (), value);
	    }

	    if (status)
	    {
		screen->setOptionForPlugin (path[0].c_str (),
					    option.name ().c_str (),
					    value);

		if (!dbus_message_get_no_reply (message))
		{
		    DBusMessage *reply;

		    reply = dbus_message_new_method_return (message);

		    dbus_connection_send (connection, reply, NULL);
		    dbus_connection_flush (connection);

		    dbus_message_unref (reply);
		}
	    }

	    return status;
	}
    }

    return false;
}

void
DbusScreen::appendSimpleOptionValue (DBusMessage       *message,
				     CompOption::Type  type,
				     CompOption::Value &value)
{
    switch (type) {
    case CompOption::TypeBool:
	{
	    dbus_bool_t b = value.b () ? TRUE : FALSE;
	    dbus_message_append_args (message,
				      DBUS_TYPE_BOOLEAN, &b,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeInt:
	{
	    int i = value.i ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_INT32, &i,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeFloat:
	{
	    double d = value.f ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_DOUBLE, &d,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeString:
	{
	    CompString string = value.s ();
	    const char *s = string.c_str ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeColor:
	{
	    CompString color = CompOption::colorToString (value.c ());
	    const char *s = color.c_str ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeKey:
	{
	    CompString key = value.action ().keyToString ();
	    const char *s = key.c_str ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeButton:
	{
	    CompString button = value.action ().buttonToString ();
	    const char *s = button.c_str ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeEdge:
	{
	    CompString edge = value.action ().edgeMaskToString ();
	    const char *s = edge.c_str ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeBell:
	{
	    dbus_bool_t bell = value.action ().bell () ? TRUE : FALSE;
	    dbus_message_append_args (message,
				      DBUS_TYPE_BOOLEAN, &bell,
				      DBUS_TYPE_INVALID);
	}
	break;
    case CompOption::TypeMatch:
	{
	    CompString match = value.match ().toString ();
	    const char *s = match.c_str ();
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	}
    default:
	break;
    }
}

void
DbusScreen::appendListOptionValue (DBusMessage       *message,
				   CompOption::Type  type,
				   CompOption::Value &value)
{
    DBusMessageIter iter;
    DBusMessageIter listIter;
    char	    sig[2];

    switch (value.listType ()) {
    case CompOption::TypeInt:
	sig[0] = DBUS_TYPE_INT32;
	break;
    case CompOption::TypeFloat:
	sig[0] = DBUS_TYPE_DOUBLE;
	break;
    case CompOption::TypeBool:
    case CompOption::TypeBell:
	sig[0] = DBUS_TYPE_BOOLEAN;
	break;
    default:
	sig[0] = DBUS_TYPE_STRING;
	break;
    }
    sig[1] = '\0';

    dbus_message_iter_init_append (message, &iter);

    if (!dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY,
					   sig, &listIter))
	return;

    foreach (CompOption::Value& val, value.list ())
    {
	switch (value.listType ()) {
	case CompOption::TypeInt:
	    {
		int i = val.i ();
		dbus_message_iter_append_basic (&listIter, sig[0], &i);
	    }
	    break;
	case CompOption::TypeFloat:
	    {
		float f = val.f ();
		dbus_message_iter_append_basic (&listIter, sig[0], &f);
	    }
	    break;
	case CompOption::TypeBool:
	    {
		dbus_bool_t b = val.b () ? TRUE : FALSE;
		dbus_message_iter_append_basic (&listIter, sig[0], &b);
	    }
	    break;
	case CompOption::TypeString:
	    {
		CompString string = val.s ();
		const char *s = string.c_str ();
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
	    }
	    break;
	case CompOption::TypeKey:
	    {
		CompString key = val.action ().keyToString ();
		const char *s = key.c_str ();
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
	    }
	    break;
	case CompOption::TypeButton:
	    {
		CompString button = val.action ().buttonToString ();
		const char *s = button.c_str ();
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
	    }
	    break;
	case CompOption::TypeEdge:
	    {
		CompString edge = val.action ().edgeMaskToString ();
		const char *s = edge.c_str ();
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
	    }
	    break;
	case CompOption::TypeBell:
	    {
		bool bell = val.action ().bell () ? true : false;
		dbus_message_iter_append_basic (&listIter, sig[0], &bell);
	    }
	    break;
	case CompOption::TypeMatch:
	    {
		CompString match = val.match ().toString ();
		const char *s = match.c_str ();
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
	    }
	    break;
	case CompOption::TypeColor:
	    {
		CompString color = CompOption::colorToString (val.c ());
		const char *s = color.c_str ();
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
	    }
	    break;
	default:
	    break;
	}
    }

    dbus_message_iter_close_container (&iter, &listIter);
}

void
DbusScreen::appendOptionValue (DBusMessage       *message,
			       CompOption::Type  type,
			       CompOption::Value &value)
{
    if (type == CompOption::TypeList)
	appendListOptionValue (message, type, value);
    else
	appendSimpleOptionValue (message, type, value);
}

/*
 * 'Get' can be used to retrieve the value of any existing option.
 *
 * Example (will retrieve the current value of command0 option):
 *
 * dbus-send --print-reply --type=method_call	    \
 * --dest=org.freedesktop.compiz		    \
 * /org/freedesktop/compiz/core/allscreens/command0 \
 * org.freedesktop.compiz.get
 */
bool
DbusScreen::handleGetOptionMessage (DBusConnection                 *connection,
				    DBusMessage                    *message,
				    const std::vector<CompString>& path)
{
    CompOption::Vector  &options = getOptionsFromPath (path);
    DBusMessage         *reply = NULL;

    foreach (CompOption& option, options)
    {
	if (option.name () == path[2])
	{
	    reply = dbus_message_new_method_return (message);
	    appendOptionValue (reply, option.type (), option.value ());
	    break;
	}
    }

    if (!reply)
	reply = dbus_message_new_error (message,
					DBUS_ERROR_FAILED,
					"No such option");

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return true;
}

/*
 * 'List' can be used to retrieve a list of available options.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call \
 * --dest=org.freedesktop.compiz	      \
 * /org/freedesktop/compiz/core/allscreens    \
 * org.freedesktop.compiz.list
 */
bool
DbusScreen::handleListMessage (DBusConnection                 *connection,
			       DBusMessage                    *message,
			       const std::vector<CompString>& path)
{
    CompOption::Vector &options = getOptionsFromPath (path);
    DBusMessage        *reply;

    reply   = dbus_message_new_method_return (message);

    foreach (CompOption& option, options)
    {
	CompString name = option.name ();
	const char *s = name.c_str ();

	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &s,
				  DBUS_TYPE_INVALID);
    }

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return true;
}

/*
 * 'GetMetadata' can be used to retrieve metadata for an option.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call		\
 * --dest=org.freedesktop.compiz			\
 * /org/freedesktop/compiz/core/allscreens/run_command0 \
 * org.freedesktop.compiz.getMetadata
 */
#if GETMETADATA_ENABLED
bool
DbusScreen::handleGetMetadataMessage (DBusConnection                 *connection,
				      DBusMessage                    *message,
				      const std::vector<CompString>& path)
{
    CompOption::Vector   options;
    DBusMessage  	 *reply = NULL;
    CompMetadata 	 *m;

    options = getOptionsFromPath (path, &m);

    foreach (CompOption *option, options)
    {
	if (strcmp (option->name ().c_str (), path[2]) == 0)
	{
/* TODO: Write xmlTextWriter for getting long screen option descriptions */
	    CompOption::Type restrictionType = option->type ();
	    const char	   *type;
	    char	   *shortDesc = NULL;
	    char	   *longDesc = NULL;
	    const char     *blankStr = "";

	    reply = dbus_message_new_method_return (message);

	    type = optionTypeToString (option->type);

	    if (m)
	    {
		if (object->type == COMP_OBJECT_TYPE_SCREEN)
		{
		    shortDesc = compGetShortScreenOptionDescription (m, option);
		    longDesc  = compGetLongScreenOptionDescription (m, option);
		}
		else
		{
		    shortDesc =
			compGetShortDisplayOptionDescription (m, option);
		    longDesc  = compGetLongDisplayOptionDescription (m, option);
		}
	    }

	    if (shortDesc)
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &shortDesc,
					  DBUS_TYPE_INVALID);
	    else
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &blankStr,
					  DBUS_TYPE_INVALID);

	    if (longDesc)
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &longDesc,
					  DBUS_TYPE_INVALID);
	    else
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &blankStr,
					  DBUS_TYPE_INVALID);

	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &type,
				      DBUS_TYPE_INVALID);

	    if (shortDesc)
		free (shortDesc);
	    if (longDesc)
		free (longDesc);

	    if (restrictionType == CompOptionTypeList)
	    {
		type = optionTypeToString (option->value.list.type);
		restrictionType = option->value.list.type;

		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &type,
					  DBUS_TYPE_INVALID);
	    }

	    switch (restrictionType) {
	    case CompOptionTypeInt:
		dbus_message_append_args (reply,
					  DBUS_TYPE_INT32, &option->rest.i.min,
					  DBUS_TYPE_INT32, &option->rest.i.max,
					  DBUS_TYPE_INVALID);
		break;
	    case CompOptionTypeFloat: {
		double min, max, precision;

		min	  = option->rest.f.min;
		max	  = option->rest.f.max;
		precision = option->rest.f.precision;

		dbus_message_append_args (reply,
					  DBUS_TYPE_DOUBLE, &min,
					  DBUS_TYPE_DOUBLE, &max,
					  DBUS_TYPE_DOUBLE, &precision,
					  DBUS_TYPE_INVALID);
	    } break;
	    default:
		break;
	    }
	    break;
	}

	option++;
    }

    if (!reply)
	reply = dbus_message_new_error (message,
					DBUS_ERROR_FAILED,
					"No such option");

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return true;
}
#endif

/*
 * 'GetPluginMetadata' can be used to retrieve metadata for a plugin.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call \
 * --dest=org.freedesktop.compiz	      \
 * /org/freedesktop/compiz		      \
 * org.freedesktop.compiz.getPluginMetadata   \
 * string:'png'
 */
/* TODO: This and xmlTextWriter to get short and long descs */

#if GET_PLUGIN_METADATA_ENABLED
bool
DbusScreen::handleGetPluginMetadataMessage (DBusConnection *connection,
				            DBusMessage    *message)
{
    DBusMessage     *reply;
    DBusMessageIter iter;
    char	    *name;
    CompPlugin	    *p, *loadedPlugin = NULL;

    if (!dbus_message_iter_init (message, &iter))
	return false;

    if (!tryGetValueWithType (&iter,
				  DBUS_TYPE_STRING,
				  &name))
	return false;

    p = findActivePlugin (name);
    if (!p)
	p = loadedPlugin = loadPlugin (name);

    if (p)
    {
	dbus_bool_t initializedPlugin = TRUE;
	char	    *shortDesc = NULL;
	char	    *longDesc = NULL;
	const char  *blankStr = "";

	reply = dbus_message_new_method_return (message);

	if (loadedPlugin)
	{
	    if (!(*p->vTable->init) (p))
		initializedPlugin = FALSE;
	}

	if (initializedPlugin && p->vTable->getMetadata)
	{
	    CompMetadata *m;

	    m = (*p->vTable->getMetadata) (p);
	    if (m)
	    {
		shortDesc = compGetShortPluginDescription (m);
		longDesc  = compGetLongPluginDescription (m);
	    }
	}

	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &p->vTable->name,
				  DBUS_TYPE_INVALID);

	if (shortDesc)
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &shortDesc,
				      DBUS_TYPE_INVALID);
	else
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &blankStr,
				      DBUS_TYPE_INVALID);

	if (longDesc)
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &longDesc,
				      DBUS_TYPE_INVALID);
	else
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &blankStr,
				      DBUS_TYPE_INVALID);

	dbus_message_append_args (reply,
				  DBUS_TYPE_BOOLEAN, &initializedPlugin,
				  DBUS_TYPE_INVALID);

	if (shortDesc)
	    free (shortDesc);
	if (longDesc)
	    free (longDesc);

	if (loadedPlugin && initializedPlugin)
	    (*p->vTable->fini) (p);
    }
    else
    {
	char *str;

	str = malloc (strlen (name) + 256);
	if (!str)
	    return false;

	sprintf (str, "Plugin '%s' could not be loaded", name);

	reply = dbus_message_new_error (message,
					DBUS_ERROR_FAILED,
					str);

	free (str);
    }

    if (loadedPlugin)
	unloadPlugin (loadedPlugin);

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return true;
}
#endif

DBusHandlerResult
DbusScreen::handleMessage (DBusConnection *connection,
			   DBusMessage    *message,
			   void           *userData)
{
    bool                    status = false;
    std::vector<CompString> path;

    if (!getPathDecomposed (dbus_message_get_path (message), path))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* root messages */
    if (path.empty ())
    {
	if (dbus_message_is_method_call (message,
					 DBUS_INTERFACE_INTROSPECTABLE,
					 "Introspect"))
	{
	    if (handleRootIntrospectMessage (connection, message))
		return DBUS_HANDLER_RESULT_HANDLED;
	}
#if GET_PLUGIN_METADATA_ENABLED
	else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
				 COMPIZ_DBUS_GET_PLUGIN_METADATA_MEMBER_NAME))
	{
	    if (handleGetPluginMetadataMessage (connection, message))
		return DBUS_HANDLER_RESULT_HANDLED;
	}
#endif

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    /* plugin message */
    else if (path.size () == 1)
    {
	if (dbus_message_is_method_call (message,
					 DBUS_INTERFACE_INTROSPECTABLE,
					 "Introspect"))
	{
	    if (handlePluginIntrospectMessage (connection, message))
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    /* screen message */
    else if (path.size () == 2)
    {
	if (dbus_message_is_method_call (message,
					 DBUS_INTERFACE_INTROSPECTABLE,
					 "Introspect"))
	{
	    if (handleScreenIntrospectMessage (connection, message, path))
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					      COMPIZ_DBUS_LIST_MEMBER_NAME))
	{
	    if (handleListMessage (connection, message, path))
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    /* option message */
    if (dbus_message_is_method_call (message, DBUS_INTERFACE_INTROSPECTABLE,
				     "Introspect"))
    {
	status = handleOptionIntrospectMessage (connection, message, path);
    }

    if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_ACTIVATE_MEMBER_NAME))
    {
	status = handleActionMessage (connection, message, path, true);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_DEACTIVATE_MEMBER_NAME))
    {
	status = handleActionMessage (connection, message, path, false);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_SET_MEMBER_NAME))
    {
	status = handleSetOptionMessage (connection, message, path);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_GET_MEMBER_NAME))
    {
	status = handleGetOptionMessage (connection, message, path);
    }
#if GETMETADATA_ENABLED
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_GET_METADATA_MEMBER_NAME))
    {
	status = handleGetMetadataMessage (connection, message, path);
    }
#endif

    if (status)
	return DBUS_HANDLER_RESULT_HANDLED;

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool
DbusScreen::processMessages (short int data)
{
    DBusDispatchStatus status;

    do
    {
	dbus_connection_read_write_dispatch (connection, 0);
	status = dbus_connection_get_dispatch_status (connection);
    }
    while (status == DBUS_DISPATCH_DATA_REMAINS);

    return true;
}

void
DbusScreen::sendChangeSignalForOption (CompOption       *o,
			               const CompString &plugin)
{
    DBusMessage       *signal;
    char	      path[256];
    CompOption::Value v;

    if (!o)
	return;

    snprintf (path, 256, "%s/%s/%s/%s", COMPIZ_DBUS_ROOT_PATH,
	     plugin.c_str (), "options", o->name ().c_str ());

    signal = dbus_message_new_signal (path,
				      COMPIZ_DBUS_SERVICE_NAME,
				      COMPIZ_DBUS_CHANGED_SIGNAL_NAME);

    appendOptionValue (signal, o->type (), o->value ());

    dbus_connection_send (connection, signal, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (signal);
}

bool
DbusScreen::getPathDecomposed (const char              *data,
			       std::vector<CompString> &path)
{
    CompString full (data);
    size_t     lastPos = 0, pos;

    path.clear ();

    while ((pos = full.find ('/', lastPos)) != CompString::npos)
    {
	CompString part = full.substr (lastPos, pos - lastPos);

	/* If we just have "/", then strip it, but don't push back
	 * an empty string at the start
	 */
	if (part.empty ())
	{
	    lastPos = pos + 1;
	    continue;
	}

	path.push_back (part);
	lastPos = pos + 1;
    }

    /* Remaining part because there was no "/" at the end of path */
    path.push_back (full.substr (lastPos, pos - lastPos).c_str ());

    if (path.size () < 3)
	return false;

    /* strip leading org.freedesktop.compiz */
    path.erase (path.begin (), path.begin () + 3);

    return true;
}

/* dbus registration */

bool
DbusScreen::registerOptions (DBusConnection *connection,
			     char	    *screenPath)
{
    std::vector<CompString> path;
    char                    objectPath[256];

    if (!getPathDecomposed (screenPath, path))
	return false;

    CompOption::Vector &options = getOptionsFromPath (path);
    if (options.empty ())
    {
	return false;
    }

    foreach (CompOption& option, options)
    {
	snprintf (objectPath, 256, "%s/%s", screenPath, option.name ().c_str ());

	dbus_connection_register_object_path (connection, objectPath,
					      &dbusMessagesVTable, 0);
    }

    return true;
}

bool
DbusScreen::unregisterOptions (DBusConnection *connection,
			       char           *screenPath)
{
    char                    objectPath[256];
    std::vector<CompString> path;

    getPathDecomposed (screenPath, path);

    CompOption::Vector &options = getOptionsFromPath (path);
    if (options.empty ())
	return false;

    foreach (CompOption& option, options)
    {
	snprintf (objectPath, 256, "%s/%s", screenPath, option.name ().c_str ());

	dbus_connection_unregister_object_path (connection, objectPath);
    }

    return true;
}

void
DbusScreen::registerPluginForScreen (DBusConnection *connection,
				     const char     *pluginName)
{
    char objectPath[256];

    /* register plugin/screen path */
    snprintf (objectPath, 256, "%s/%s/screen%d",
	      COMPIZ_DBUS_ROOT_PATH, pluginName, screen->screenNum ());
    dbus_connection_register_object_path (connection, objectPath,
					  &dbusMessagesVTable, screen->dpy ());
}

void
DbusScreen::registerPluginsForScreen (DBusConnection *connection)
{
    CompPlugin::List pl = CompPlugin::getPlugins ();
    char             path[256];

    foreach (CompPlugin *p, pl)
    {
	const char *plugin = p->vTable->name ().c_str ();

	snprintf (path, 256, "%s/%s/screen%d",
		  COMPIZ_DBUS_ROOT_PATH, plugin, screen->screenNum ());
	registerPluginForScreen (connection, plugin);
	registerOptions (connection, path);
    }
}

void
DbusScreen::unregisterPluginForScreen (DBusConnection *connection,
				       const char     *pluginName)
{
    char objectPath[256];

    snprintf (objectPath, 256, "%s/%s/screen%d", COMPIZ_DBUS_ROOT_PATH,
	      pluginName, screen->screenNum ());

    unregisterOptions (connection, objectPath);
    dbus_connection_unregister_object_path (connection, objectPath);
}

void
DbusScreen::unregisterPluginsForScreen (DBusConnection *connection)
{
    CompPlugin::List pl = CompPlugin::getPlugins ();

    foreach (CompPlugin *p, pl)
    {
	const char *plugin = p->vTable->name ().c_str ();
	unregisterPluginForScreen (connection, plugin);
    }
}

bool
DbusScreen::initPluginForScreen (CompPlugin *p)
{
    char objectPath[256];

    snprintf (objectPath, 256, "%s/%s/screen%d", COMPIZ_DBUS_ROOT_PATH,
	      p->vTable->name ().c_str (), screen->screenNum ());
    registerOptions (connection, objectPath);

    screen->initPluginForScreen (p);

    return true;
}

bool
DbusScreen::setOptionForPlugin (const char        *plugin,
				const char        *name,
				CompOption::Value &value)
{
    bool status = screen->setOptionForPlugin (plugin, name, value);

    if (status)
    {
	CompPlugin *p;

	p = CompPlugin::find (plugin);
	if (p && p->vTable)
	{
	    CompOption::Vector &options = p->vTable->getOptions ();
	    sendChangeSignalForOption (CompOption::findOption (options, name),
				       p->vTable->name ());

	    if (p->vTable->name () == "core" &&
		strcmp (name, "active_plugins") == 0)
	    {
		unregisterPluginsForScreen (connection);
		registerPluginsForScreen (connection);
	    }
	}
    }

    return status;
}

void
DbusScreen::sendPluginsChangedSignal (const char *name)
{
    DBusMessage *signal;

    signal = dbus_message_new_signal (COMPIZ_DBUS_ROOT_PATH,
				      COMPIZ_DBUS_SERVICE_NAME,
				      COMPIZ_DBUS_PLUGINS_CHANGED_SIGNAL_NAME);

    dbus_connection_send (connection, signal, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (signal);
}

/* We might have to hook initScreen here instead of the screen ctor */
DbusScreen::DbusScreen (CompScreen *screen) :
    PluginClassHandler <DbusScreen, CompScreen> (screen)
{
    DBusError         error;
    dbus_bool_t       status;
    int               fd, ret, mask;
    char              *home;
    char              objectPath[256];
    FdWatchCallBack   fdCb;
    FileWatchCallBack fileCb;

    dbus_error_init (&error);

    connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_bus_get error: %s", error.message);

	dbus_error_free (&error);
	setFailed ();
    }

    ret = dbus_bus_request_name (connection,
				 COMPIZ_DBUS_SERVICE_NAME,
				 DBUS_NAME_FLAG_REPLACE_EXISTING |
				 DBUS_NAME_FLAG_ALLOW_REPLACEMENT,
				 &error);

    if (dbus_error_is_set (&error))
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_bus_request_name error: %s", error.message);

	/* dbus_connection_unref (dc->connection); */
	dbus_error_free (&error);
	setFailed ();
    }

    dbus_error_free (&error);

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_bus_request_name reply is not primary owner");

	/* dbus_connection_unref (dc->connection); */
	setFailed ();
    }

    status = dbus_connection_get_unix_fd (connection, &fd);
    if (!status)
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_connection_get_unix_fd failed");

	/* dbus_connection_unref (dc->connection); */
	setFailed ();
    }

    fdCb = boost::bind (&DbusScreen::processMessages, this, _1);
    watchFdHandle = screen->addWatchFd (fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					fdCb);

    mask = NOTIFY_CREATE_MASK | NOTIFY_DELETE_MASK | NOTIFY_MOVE_MASK;

    fileCb = boost::bind (&DbusScreen::sendPluginsChangedSignal, this, _1);
    fileWatch[DBUS_FILE_WATCH_CURRENT] = screen->addFileWatch (".", mask,
							       fileCb);
    fileWatch[DBUS_FILE_WATCH_PLUGIN]  = screen->addFileWatch (PLUGINDIR,
							       mask, fileCb);
    fileWatch[DBUS_FILE_WATCH_HOME] = 0;

    home = getenv ("HOME");
    if (home)
    {
	CompString pluginDir (home);
	pluginDir += "/";
	pluginDir += HOME_PLUGINDIR;

	fileWatch[DBUS_FILE_WATCH_HOME] =
	    screen->addFileWatch (pluginDir.c_str (), mask, fileCb);
    }

    ScreenInterface::setHandler (screen);

    /* register the objects */
    dbus_connection_register_object_path (connection,
					  COMPIZ_DBUS_ROOT_PATH,
					  &dbusMessagesVTable, 0);

    snprintf (objectPath, 256, "%s/%s/screen%d", COMPIZ_DBUS_ROOT_PATH,
	      "core", screen->screenNum ());

    registerPluginForScreen (connection, "core");
    registerPluginsForScreen (connection);
    registerOptions (connection, objectPath);
}

DbusScreen::~DbusScreen ()
{
    for (int i = 0; i < DBUS_FILE_WATCH_NUM; i++)
	screen->removeFileWatch (fileWatch[i]);

    screen->removeWatchFd (watchFdHandle);

    /*
      can't unref the connection returned by dbus_bus_get as it's
      shared and we can't know if it's closed or not.

      dbus_connection_unref (connection);
    */

    dbus_bus_release_name (connection, COMPIZ_DBUS_SERVICE_NAME, NULL);

    unregisterPluginForScreen (connection, "core");
    unregisterPluginsForScreen (connection);
}

bool
DbusPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	return true;

    return false;
}

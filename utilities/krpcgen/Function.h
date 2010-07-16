/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Function class.
 */

#ifndef __FUNCTION_H
#define __FUNCTION_H

#include <list>
#include <stdint.h>
#include <string>

#include "Type.h"

/** Class representing a function/event. */
class Function {
public:
	/** Structure containing details of a parameter. */
	struct Parameter {
		Type *type;		/**< Type of the parameter. */
		std::string name;	/**< Name of the parameter. */
		bool out;		/**< Whether is an output parameter. */
	};

	/** Type of the parameter list. */
	typedef std::list<Parameter> ParameterList;

	/** Construct the function.
	 * @param name		Name of the function.  */
	Function(const char *name) : m_name(name) {}

	void dump() const;
	bool addParameter(Type *type, const char *name, bool out);

	/** Get the name of the function.
	 * @return		Reference to type name. */
	const std::string &getName() const { return m_name; }

	/** Get the message ID of the function.
	 * @return		Message ID of the function. */
	uint32_t getMessageID() const { return m_id; }

	/** Set the message ID of the function.
	 * @param id		ID to set. */
	void setMessageID(uint32_t id) { m_id = id; }

	/** Get the argument list.
	 * @return		Reference to argument list. */
	const ParameterList &getParameters() const { return m_params; }
private:
	std::string m_name;		/**< Name of the function. */
	uint32_t m_id;			/**< Message ID of the function. */
	ParameterList m_params;		/**< List of parameters. */
};

#endif /* __FUNCTION_H */

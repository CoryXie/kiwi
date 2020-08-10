/*
 * Copyright (C) 2010-2014 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               AMD64 system call code generator.
 */

#include "amd64_target.h"

using namespace std;

/** Add this target's types to the type map.
 * @param map           Map to add to. */
void AMD64Target::add_types(TypeMap &map) {
    map["int"] = Type(1);
    map["uint"] = Type(1);
    map["char"] = Type(1);
    map["bool"] = Type(1);
    map["ptr_t"] = Type(1);
    map["size_t"] = Type(1);
    map["ssize_t"] = Type(1);
    map["int8_t"] = Type(1);
    map["int16_t"] = Type(1);
    map["int32_t"] = Type(1);
    map["int64_t"] = Type(1);
    map["uint8_t"] = Type(1);
    map["uint16_t"] = Type(1);
    map["uint32_t"] = Type(1);
    map["uint64_t"] = Type(1);
}

/** Generate the system call code.
 * @param stream        Stream to write output to.
 * @param calls         List of system calls. */
void AMD64Target::generate(std::ostream &stream, const SyscallList &calls) {
    stream << "/* This file is automatically generated. Do not edit! */" << endl;
    for (const Syscall *call : calls) {
        string name = call->name();
        if (call->attributes() & Syscall::kWrappedAttribute)
            name = '_' + name;

        /* The code is the same regardless of parameter count. The kernel
         * follows the AMD64 ABI for parameter passing (if there are more than
         * 6 parameters, the remaining parameters are passed on the stack),
         * except that RCX is used by SYSCALL so it is moved to R10. */
        stream << endl;
        if (call->attributes() & Syscall::kHiddenAttribute)
            stream << ".hidden " << name << endl;
        stream << ".global " << name << endl;
        stream << ".type " << name << ", @function" << endl;
        stream << name << ':' << endl;
        stream << "     movq    %rcx, %r10" << endl;
        stream << "     movq    $" << call->id() << ", %rax" << endl;
        stream << "     syscall" << endl;
        stream << "     ret" << endl;
        stream << ".size " << name << ", .-" << name << endl;
    }
}

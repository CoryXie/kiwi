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
 * @brief		Test application.
 */

#include <kiwi/Support/Utility.h>
#include <kiwi/Object.h>
#include <kiwi/Signal.h>

#include <kernel/status.h>
#include <kernel/thread.h>

#include <iostream>
#include <string>

using namespace kiwi;
using namespace std;

class Foo : public Object {
public:
	Signal<int, const char *, const std::string &> MySignal;
	void CallSignal();
	void Callback(int x, const char *str1, const std::string &str2);
};

void Foo::CallSignal() {
	MySignal(42, "Hello World", "!!!");
	MySignal(1337, "Goodbye World", ":)");
}

void Foo::Callback(int x, const char *str1, const std::string &str2) {
	cout << x << ' ' << str1 << ' ' << str2 << endl;
}

static void callback(int x, const char *str1, const std::string &str2) {
	cout << str2 << ' ' << str1 << ' ' << x << endl;
}

extern "C" void malloc_stats(void);

int main(int argc, char **argv) {
	malloc_stats();
	cout << endl;
	{
		Foo foop;
		foop.MySignal.Connect(&foop, &Foo::Callback);
		foop.MySignal.Connect(callback);
		foop.MySignal.Connect(callback);
		foop.MySignal.Connect(&foop, &Foo::Callback);
		foop.MySignal.Connect(callback);
		foop.MySignal.Connect(&foop, &Foo::Callback);
		malloc_stats();
		cout << endl;
		foop.CallSignal();
	}
	cout << endl;
	malloc_stats();
	while(1);
}

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "cc.h"

static int rank = 0;

void *
talk_routine(Coroutine *self, void *arg)
{
	char const *who = arg;
	cc_yield(self, "%s says hello.");

	char buf[100];

	sprintf(buf, "%s sets the table.", who);
	cc_yield(self, buf);

	sprintf(buf, "%s sits down.", who);
	cc_yield(self, buf);

	int i = 0;
	while (!self->cancelled) {
		if (3 <= i) {
			sprintf(buf, "%s finishes #%d. 🏁", who, ++rank);
			cc_return(self, buf);
		}

		++i;

		int n = sprintf(buf, "%s ate %d apples.", who, i);
		for (int k = 0; k < i; ++k)
			n += sprintf(buf + n, "%s", "🍎");

		cc_yield(self, buf);
	}

	sprintf(buf, "%s runs away.", who);
	cc_return(self, buf);
}

int
main()
{
	CC_DECLARE_MAIN(executor);

	srand(time(NULL));

	static char const *WHO[] = {
		"Samantha",
		"Simon",
		"Duck",
		"Fox",
	};
	enum { NWHO = sizeof(WHO) / sizeof(*WHO) };

	int m = 12 + rand() % 12;

	int nalive = 0;
	char stack[NWHO][8192];
	Coroutine creature[NWHO];

	puts("Hello everybody on the annual apple eating competition.");

	for (int i = 0; i < NWHO; ++i) {
		++nalive;
		creature[i].stack = stack[i];
		creature[i].stack_size = sizeof(stack[i]);
		if (cc_create(&creature[i], talk_routine) < 0)
			abort();

		cc_resume(&executor, &creature[i], (void *)WHO[i]);
	}

	int j = 0;
	while (nalive) {
		int i = rand() % NWHO;
		if (creature[i].done)
			continue;

		if (m == j) {
			puts("A big bear come. 🐻");
			for (int k = 0; k < NWHO; ++k)
				creature[k].cancelled = 1;
		}

		char const *s = cc_resume(&executor, &creature[i], NULL);

		printf("%d: %s\n", j++, s);

		if (creature[i].done)
			--nalive;
	}

	puts("Party ended.");
}

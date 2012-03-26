struct pinfo {
	int pid;
	int ppid;
};

/* Linux */
void machine_init ();
void for_each_pinfo (void (*func) (struct pinfo *info, void *data),void *data);

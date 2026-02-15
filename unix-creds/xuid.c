#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

void show_creds(void);

#if USE_SETRESUID
int setxuid(uid_t uid, uid_t suid){
        return setresuid(uid, uid, suid);
}
int setxgid(gid_t gid, gid_t sgid){
        return setresgid(gid, gid, sgid);
}
int drop_saved_uid(){
        return setresuid(-1, -1, geteuid());
}
#else
int setxuid(uid_t uid, uid_t svuid){
	if(setreuid(uid, svuid)) return -1;
	return seteuid(uid);
}
int setxgid(gid_t gid, gid_t sgid){
	if(setregid(gid, sgid)) return -1;
	return setegid(gid);
}
int drop_saved_uid(){
	uid_t euid = geteuid();
	if(setreuid(euid, euid)) return -1;
	return setuid(euid);
}
#endif

long getnum(const char *s){
	char *e; long l;
	errno = 0;
	l = strtol(s, &e, 0);
	if(*e || errno) errx(1, "bad number %s", s);
	return l;
}

void sigh(int s){}
int main(int ac, char **av){
	uid_t uid, svuid;
	int do_pause = 0;

	show_creds();
	if(ac > 1 && av[1][0] == '-'){
		ac--; av++; do_pause = 1;
	}
	uid = ac > 1 ? getnum(av[1]) : 77;
	svuid = ac > 2 ? getnum(av[2]) : geteuid();

	/* set the real and effective user id to uid,
	   and saved-set-user-id to svuid */
	if(setxuid(uid, svuid))
		err(1, "setxuid");
	show_creds();


	/* switch to the saved-set-user-id and back */
	if(seteuid(svuid))
		err(1, "seteuid %d", svuid);
	show_creds();
	if(seteuid(uid))
		err(1, "seteuid %d", uid);
	show_creds();

	/* drop the saved-set-user-id */
	drop_saved_uid();
	show_creds();

	if(do_pause){
		signal(SIGINT, sigh);
		pause();
	}

	/* check if it's really dropped */
	if(setreuid(svuid, uid) == 0)
		errx(1, "setreuid(%d, %d) SUCCEEDED!", svuid, uid);
	if(seteuid(svuid) == 0)
		errx(1, "seteuid(%d) SUCCEEDED!", svuid);
	if(setuid(svuid) == 0)
		errx(1, "setuid(%d) SUCCEEDED!", svuid);
	show_creds();

	return 0;
}

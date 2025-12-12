#define _GNU_SOURCE
#if __NetBSD__
#define _KMEMUSER
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* no getresuid, use sysctl(2) */
#if __APPLE__ || __NetBSD__
#include <sys/sysctl.h>
#if __NetBSD__
#define _pcred	ki_pcred
#define _ucred	ki_ucred
#endif
void show_creds(void){
	int i;
	struct kinfo_proc kp;
	struct _pcred *pc;
	struct _ucred *uc;
	size_t size = sizeof kp;
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
	if(sysctl(mib, 4, &kp, &size, 0, 0))
		err(1, "sysctl");
	pc = &kp.kp_eproc.e_pcred;
	uc = &kp.kp_eproc.e_ucred;
	i = printf("resuid %2d %2d %2d", pc->p_ruid, uc->cr_uid, pc->p_svuid);
	printf("%*s", 24 - i, "");
	i = printf("resgid %2d %2d %2d", pc->p_rgid, uc->cr_gid, pc->p_svgid);
	printf("%*sgroups", 25 - i, "");
	for(i = 0; i < uc->cr_ngroups; i++) printf(" %d", uc->cr_groups[i]);
	printf("\n");
}

/* older solarises don't have getresuid; use /proc/PID/cred */
#elif __sun
#include <stddef.h>
struct prcred {
	uid_t euid;      /* effective user id */
	uid_t ruid;      /* real user id */
	uid_t suid;      /* saved user id (from exec) */
	gid_t egid;      /* effective group id */
	gid_t rgid;      /* real group id */
	gid_t sgid;      /* saved group id (from exec) */
	int ngroups;     /* number of supplementary groups */
	gid_t groups[NGROUPS_MAX]; /* array of supplementary groups */
};
void show_creds(void){
	char b[256]; int i, fd; struct prcred c;
	snprintf(b, sizeof b, "/proc/%d/cred", getpid());
	if((fd = open(b, O_RDONLY)) == -1) err(1, "open %s", b);
	if(read(fd, &c, sizeof c) < offsetof(struct prcred, groups))
		err(1, "read %s", b); 
	close(fd);
	i = printf("resuid %2d %2d %2d", c.ruid, c.euid, c.suid);
	printf("%*s", 24 - i, "");
	i = printf("resgid %2d %2d %2d", c.rgid, c.egid, c.sgid);
	printf("%*sgroups", 25 - i, "");
	for(i = 0; i < c.ngroups; i++) printf(" %d", c.groups[i]);
	printf("\n");
}


/* have getresuid(2) */
#elif __linux__ || __FreeBSD__ || __OpenBSD__
void show_creds(void){
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid, groups[NGROUPS_MAX];
	int i, n;
	if(getresuid(&ruid, &euid, &suid)) err(1, "getresuid");
	if(getresgid(&rgid, &egid, &sgid)) err(1, "getresgid");
	if((n = getgroups(sizeof groups/sizeof*groups, groups)) < 0)
		err(1, "getgroups");
	i = printf("resuid %2d %2d %2d", ruid, euid, suid);
	printf("%*s", 24 - i, "");
	i = printf("resgid %2d %2d %2d", rgid, egid, sgid);
	printf("%*sgroups", 25 - i, "");
	for(i = 0; i < n; i++) printf(" %d", groups[i]);
	printf("\n");
}

/* no getresuid(2), just omit the saved-set-uid info */
#else
void show_creds(void){
	uid_t ruid = getuid(), euid = geteuid();
	gid_t rgid = getgid(), egid = getegid(), groups[NGROUPS_MAX];
	int i, n;
	if((n = getgroups(sizeof groups/sizeof*groups, groups)) < 0)
		err(1, "getgroups");
	i = printf("resuid %2d %2d ?", ruid, euid);
	printf("%*s", 24 - i, "");
	i = printf("resgid %2d %2d ?", rgid, egid);
	printf("%*sgroups", 25 - i, "");
	for(i = 0; i < n; i++) printf(" %d", groups[i]);
	printf("\n");
}
#endif

/**********************************************/
int setxuid(uid_t uid, uid_t svuid){
	if(setreuid(uid, svuid)) return -1;
	return seteuid(uid);
}
int setxgid(gid_t gid, gid_t sgid){
	if(setregid(gid, sgid)) return -1;
	return setegid(gid);
}
int drop_saved_uid(uid_t svuid){
	uid_t euid = geteuid();
	if(setreuid(euid, svuid)) return -1;
	if(setreuid(euid, euid)) return -1;
	return setuid(euid);
}
/**********************************************/

long getnum(const char *s){
	char *e; long l;
	errno = 0;
	l = strtol(s, &e, 0);
	if(*e || errno) errx(1, "bad number %s", s);
	return l;
}

int main(int ac, char **av){
	uid_t uid, svuid;

	show_creds();
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
	drop_saved_uid(svuid);
	show_creds();

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

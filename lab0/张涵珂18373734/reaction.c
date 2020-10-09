#include "pintos_thread.h"

// Forward declaration. This function is implemented in reaction-runner.c,
// but you needn't care what it does. Just be sure it's called when
// appropriate within reaction_o()/reaction_h().
void make_water();

struct reaction {
	int h_num;
	struct lock *lock;
	struct condition *h;
	struct condition *o;
};

void
reaction_init(struct reaction *reaction)
{
	reaction->h = malloc(sizeof(struct condition));
	reaction->o = malloc(sizeof(struct condition));
	reaction->lock = malloc(sizeof(struct lock));
	reaction->h_num = 0;
	lock_init(reaction->lock);
	cond_init(reaction->h);
	cond_init(reaction->o);
}

void
reaction_h(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	// 生产H原子 
	reaction->h_num++;
	// 唤醒生产线程进行产水 
	cond_signal(reaction->o, reaction->lock);
	// 生产完毕 自身阻塞 
	cond_wait(reaction->h, reaction->lock);
	lock_release(reaction->lock);
}

void
reaction_o(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	// 如果H原子不足 则生产线程阻塞 
	while(reaction->h_num < 2){
		cond_wait(reaction->o, reaction->lock);
	}
	// 生产水 
	reaction->h_num -= 2;
	make_water();
	// 唤醒两个H原子阻塞线程 因为需要两个H线程才和一个O生产线程一起返回 
	cond_signal(reaction->h, reaction->lock);
	cond_signal(reaction->h, reaction->lock);
	lock_release(reaction->lock);
}

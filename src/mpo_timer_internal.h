//
// Created by Matt on 10/14/2019.
//

#ifndef MPO2_MPO_TIMER_INTERNAL_H
#define MPO2_MPO_TIMER_INTERNAL_H

#include <mpolib/mpo_timer.h>

class MpoTimer : public IMpoTimer, public MpoDeleter
{
public:
	static IMpoTimerSPtr CreateInstance();

	unsigned int GetCurValMs();
	unsigned int GetElapsedMs(unsigned int uOldTime);

private:
	MpoTimer() {}
	virtual ~MpoTimer() {}

	void DeleteInstance() { delete this; }
};


#endif //MPO2_MPO_TIMER_INTERNAL_H

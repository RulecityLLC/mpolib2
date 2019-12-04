#ifndef MPO_DELETER_H
#define MPO_DELETER_H

// if we're on C++11, then shared_ptr is standard
#if __cplusplus > 199711L

#include <memory>
using std::shared_ptr;

#else

// force compile failure
C++11 is required

#endif

class MpoDeleter
{
protected:

	// In each concrete class, insert "void DeleteInstance() { delete this; }"
	// I did some experimentation and found that this virtual method must indeed be implemented in each concrete class.
	// I could not find a way to properly call the concrete class's destructor by deleting from this MpoDeleter class.
	virtual void DeleteInstance() = 0;

	class deleter;
	friend class deleter;

	// THIS DELETE CODE FROM BOOST EXAMPLE
	class deleter
	{
	public:
		void operator()(MpoDeleter *p)
		{
			p->DeleteInstance();
		}
	};
	// END BOOST EXAMPLE

};

#endif // MPO_DELETER_H

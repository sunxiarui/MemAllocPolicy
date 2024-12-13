#ifndef COMMON_INTERFACE_H
#define	COMMON_INTERFACE_H
//#pragma once
//! Base class for types that should not be assigned.
class no_assign {
	// Deny assignment
	void operator=( const no_assign& );
public:
#if __GNUC__
	//! Explicitly define default construction, because otherwise gcc issues gratuitous warning.
	no_assign() {}
#endif /* __GNUC__ */
};

class no_copy: no_assign {
	//! Deny copy construction
	no_copy( const no_copy& );
public:
	//! Allow default construction
	no_copy() {}
};




class IConstBuffer: no_copy{
public:
	virtual ~IConstBuffer() {}
	virtual const char* get_data() const =0;
	virtual long get_size() const =0;
};

class IBuffer:public IConstBuffer {
public:
	virtual ~IBuffer() {}
	virtual char* get_data() =0;
};

#endif	/* COMMON_INTERFACE_H */


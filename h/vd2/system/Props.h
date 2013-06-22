#ifndef f_SYSTEM_PROPS_H
#define f_SYSTEM_PROPS_H

struct PropDef {
	enum PropItemType {
		kNull,
		kInt,
		kDouble,
		kString,
		kBool,

		kAutodialogItemType_Force16 = 0x7FFF
	};

	enum PropControlType {
		kDefault,
		kRadio,
		kStatic,
		kAutodialogControlType_Force16 = 0x7FFF
	};

	PropItemType		type;			// Type of item
	PropControlType		controltype;	// Control type (expression) for item
	const char			*vname;			// Variable name for item
	const char			*name;			// Long name of item
	const char			*help;			// Help for item
	int					lo, hi;
	const char			*enabler;
	const char			*effector;
};

union PropVal {
	int			i;
	double		d;
	const char *s;
	bool		f;
};

class IProps {
public:
	virtual const IProps& operator=(const IProps&)=0;
	virtual IProps *clone() const = 0;
	virtual const PropVal& operator[](int id) const =0;
	virtual void setInt(int id, int i) throw() =0;
	virtual void setDbl(int id, double r) throw() =0;
	virtual void setStr(int id, const char *s) throw() =0;
	virtual void setBool(int id, bool f) throw() =0;
	virtual const PropDef *getDef(int id) const throw() = 0;
	virtual int getType(int id) const throw() = 0;
	virtual int getCount() const throw() =0;
	virtual int lookup(const char *s) const throw() = 0;
	virtual char *serialize(long& len) const throw() = 0;
	virtual void deserialize(const char *s) throw() = 0;
};

class Props : public IProps {
protected:
	const PropDef *const pDef;
	PropVal *pData;
	int nItems;

public:
	Props(const PropDef *);
	Props(const Props&);
	~Props();

	const IProps& operator=(const IProps&);
	IProps *clone() const;
	const PropVal& operator[](int id) const;
	void setInt(int id, int i) throw();
	void setDbl(int id, double r) throw();
	void setStr(int id, const char *s) throw();
	void setBool(int id, bool f) throw();
	const PropDef *getDef(int id) const throw();
	int getType(int id) const throw();
	int getCount() const throw();
	int lookup(const char *s) const throw();
	char *serialize(long& len) const throw();
	void deserialize(const char *s) throw();
};

#endif

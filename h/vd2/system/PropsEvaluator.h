#ifndef f_PROPSEVALUATOR_H
#define f_PROPSEVALUATOR_H

class PropsEvaluator {
protected:
	struct PropEVal {
		int			type;
		int			parens;
		int			op;
		PropVal		v;
	};

	static const int PropsEvaluator::prec[];

	const IProps		**ppProps;
	PropEVal		*stack;
	int				sp, sp_max;

	const char *evaluate(const char *szExp, bool bFunction);
	void	reduce();
	void	convert(PropEVal&, int);
	const char *lookupvar(const char *s);

public:
	PropsEvaluator(const IProps **ppProps);
	~PropsEvaluator();

	int		evaluateInt(const char *szExp);
	double	evaluateDouble(const char *szExp);
	bool	evaluateBool(const char *szExp);
	char *	evaluateString(const char *szExp);
};

#endif

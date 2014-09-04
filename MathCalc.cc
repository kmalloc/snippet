#include "MathCalc.h"

#include <cmath>

#include <string>
#include <sstream>

static inline bool ToDouble(const std::string& s, double& d)
{
    std::istringstream iss(s);
    return iss >> d;
}

enum TokenType
{
    Token_None    = 0,
    Token_OpBin   = 1,
    Token_OpUnary = 2,
};

struct OperInfo
{
    bool unary;

    // currently all operators are left associated
    bool left_assoc;

    int precedence;
};

struct MathCalculatorInfo
{
    std::set<char> op_char;

    // all the operator supported.
    std::map<MathCalculator::OpType, OperInfo> op_all;
    std::map<std::string, MathCalculator::OpType> op_binary;
    std::map<std::string, MathCalculator::OpType> op_unary;
};

static MathCalculatorInfo InitMathCalc()
{
    MathCalculatorInfo op;
    op.op_char.insert('+');
    op.op_char.insert('-');
    op.op_char.insert('*');
    op.op_char.insert('/');

    op.op_binary["+"] = MathCalculator::op_id_add;
    op.op_binary["-"] = MathCalculator::op_id_sub;
    op.op_binary["*"] = MathCalculator::op_id_mul;
    op.op_binary["/"] = MathCalculator::op_id_div;
    op.op_unary["-"] = MathCalculator::op_id_neg;

    OperInfo add = {false, true, 1};
    op.op_all[MathCalculator::op_id_add] = add;
    OperInfo sub = {false, true, 1};
    op.op_all[MathCalculator::op_id_sub] = sub;
    OperInfo mul = {false, true, 2};
    op.op_all[MathCalculator::op_id_mul] = mul;
    OperInfo div = {false, true, 2};
    op.op_all[MathCalculator::op_id_div] = div;
    OperInfo neg = {true, true, 3};
    op.op_all[MathCalculator::op_id_neg] = neg;

    return op;
}

static const MathCalculatorInfo g_math_op = InitMathCalc();

// class definition
MathCalculator::MathCalculator()
    :m_reference(NULL)
{
}

MathCalculator::MathCalculator(const std::set<std::string>* ref)
    :m_reference(ref)
{
}

int MathCalculator::IsOperator(const std::string& c)
{
    int ret = 0;
    std::map<std::string, OpType>::const_iterator it = g_math_op.op_binary.find(c);
    if (it != g_math_op.op_binary.end()) ret |= Token_OpBin;

    it = g_math_op.op_unary.find(c);
    if (it != g_math_op.op_unary.end()) ret |= Token_OpUnary;

    return ret;
}

bool MathCalculator::IsValidBinOperator(const std::vector<std::string>& tokens, int i)
{
    // a valid binary operator is between a pair of tokens.
    return !(i <= 0 || i >= tokens.size() - 1
            || IsOperator(tokens[i - 1]) || IsOperator(tokens[i + 1]));
}

bool MathCalculator::IsValidUnaryOperator(const std::vector<std::string>& tokens, int i)
{
    // a valid unary operator is first token of the expression.
    // or the first token within the parenthesis
    return (i == 0 || tokens[i - 1] == "(") &&
        (i < tokens.size() - 1 && !IsOperator(tokens[i + 1]));
}

bool MathCalculator::IsValidOperand(const std::vector<std::string>& tokens, int i)
{
    // a valid operand must stay between a pair of operator.
    // or being the first token or last token.
    return (i == 0 || (tokens[i - 1] == "(" || IsOperator(tokens[i - 1]))
            || (i == tokens.size() - 1 || IsOperator(tokens[i + 1]) || tokens[i + 1] == ")"));
}

bool MathCalculator::IsOperatorChar(char c)
{
    return g_math_op.op_char.find(c) != g_math_op.op_char.end();
}

bool MathCalculator::IsInternalMetaChar(char c)
{
    return c == ')' || c == '(';
}

bool MathCalculator::IsMetaChar(char c)
{
    return IsInternalMetaChar(c) || IsOperatorChar(c);
}

std::string MathCalculator::ExtractToken(const char*& s, const char* e)
{
    while (s < e && std::isspace(*s)) ++s;

    if (s >= e) return "";

    const char* p = s;

    if (IsOperatorChar(*s))
    {
        // extract operator
        while ((s < e) && IsOperatorChar(*s)) ++s;
    }
    else if (IsInternalMetaChar(*s))
    {
        // extract internal token.
        return std::string(1, *s++);
    }
    else
    {
        // extract operand token.
        while (s < e && !IsOperatorChar(*s) && !std::isspace(*s) && !IsInternalMetaChar(*s)) ++s;
    }

    return std::string(p, s - p);
}

bool MathCalculator::GenPostfixExp(const std::vector<std::string>& tokens,
        std::vector<TokenItem>& postExp, std::string& err)
{
    std::vector<OpType> opStack;
    opStack.reserve(16);

    for (int i = 0; i < tokens.size(); ++i)
    {
        int type = IsOperator(tokens[i]);
        if (type)
        {
            OpType op_id;
            if ((type & Token_OpBin) && IsValidBinOperator(tokens, i))
            {
                op_id = g_math_op.op_binary.find(tokens[i])->second;
            }
            else if ((type & Token_OpUnary) && IsValidUnaryOperator(tokens, i))
            {
                op_id = g_math_op.op_unary.find(tokens[i])->second;
            }
            else
            {
                err = "invalid expression, unrecognized operator:" + tokens[i];
                return false;
            }

            int prec = g_math_op.op_all.find(op_id)->second.precedence;
            while (!opStack.empty())
            {
                OpType op_prev_id = opStack[opStack.size() - 1];

                int op_old_prec = g_math_op.op_all.find(op_prev_id)->second.precedence;
                bool assoc = g_math_op.op_all.find(op_prev_id)->second.left_assoc;

                TokenItem item = {op_prev_id};
                if (op_old_prec > prec || (prec == op_old_prec && assoc))
                {
                    postExp.push_back(item);
                    opStack.pop_back();
                }
                else
                {
                    break;
                }
            }

            opStack.push_back(op_id);
        }
        else if (tokens[i] == "(")
        {
            ++i;
            std::vector<std::string> sub_group;
            while (i < tokens.size() && tokens[i] != ")")
            {
                sub_group.push_back(tokens[i]);
                ++i;
            }

            if (sub_group.empty())
            {
                err = "invalid expression, parenthesis does not match.";
                return false;
            }

            if (!GenPostfixExp(sub_group, postExp, err)) return false;
        }
        else
        {
            double d;
            if (!IsValidOperand(tokens, i) ||
                    (m_reference && m_reference->find(tokens[i]) == m_reference->end()
                     && !ToDouble(tokens[i], d)))
            {
                err = "invalid expression, unrecognized operand:" + tokens[i];
                return false;
            }

            TokenItem item = {op_id_none, tokens[i]};
            postExp.push_back(item);
        }
    }

    while (!opStack.empty())
    {
        TokenItem item = {opStack[opStack.size() - 1]};
        postExp.push_back(item);
        opStack.pop_back();
    }

    return true;
}

bool MathCalculator::ParseExpression(const std::string& exp, std::string& err)
{
    if (exp.empty())
    {
        err = "reference values not specified";
        return false;
    }

    const char* s = &exp[0];
    const char* e = s + exp.size();

    m_tokens.clear();

    while (s < e)
    {
        std::string tok = ExtractToken(s, e);
        if (!tok.empty()) m_tokens.push_back(tok);
    }

    if (m_tokens.empty())
    {
        err = "expression: " + exp + " is invalid.\n";
        return false;
    }

    std::string err2;
    m_postFixExp.clear();
    if (!GenPostfixExp(m_tokens, m_postFixExp, err2))
    {
        err = err2;
        return false;
    }

    return true;
}

double MathCalculator::GenValue(const std::map<std::string, double>& refColumn, std::string& err) const
{
    std::vector<double> stack;
    stack.reserve(16);

    for (size_t i = 0; i < m_postFixExp.size(); ++i)
    {
        OpType t = m_postFixExp[i].op;
        if (t)
        {
            double val;
            double operand2 = stack[stack.size() - 1];
            stack.pop_back();

            double operand1 = 0;
            const OperInfo& op = g_math_op.op_all.find(t)->second;

            if (!op.unary)
            {
                operand1 = stack[stack.size() - 1];
                stack.pop_back();
            }

            if (t == op_id_add)
            {
                val = operand1 + operand2;
            }
            else if (t == op_id_sub)
            {
                val = operand1 - operand2;
            }
            else if (t == op_id_mul)
            {
                val = operand1 * operand2;
            }
            else if (t == op_id_neg)
            {
                val = -operand2;
            }
            else if (t == op_id_div)
            {
                val = operand1 / operand2;
            }
            else
            {
                assert(0);
            }

            stack.push_back(val);
        }
        else
        {
            const std::string& s = m_postFixExp[i].token;
            std::map<std::string, double>::const_iterator cit = refColumn.find(s);

            double d;
            if (cit != refColumn.end())
            {
                d = cit->second;
            }
            else if (!ToDouble(s, d))
            {
                err = "operand:" + s + " not exists in gauge.\n";
                return 0;
            }

            stack.push_back(d);
        }
    }

    if (stack.size() != 1)
    {
        err = "invalid formula";
        return 0;
    }

    return stack[0];
}

// Get value directly from a match expression like: "20.0000- 30.0000+ 5.000", return -5.0000
double MathCalculator::GenValue(const std::string& exp, std::string& err)
{
    std::map<std::string, double> refValue;

    err.clear();
    if (!ParseExpression(exp, err))
    {
        return 0.0;
    }

    for (int i = 0; i < m_tokens.size(); ++i)
    {
        std::string token = m_tokens[i];
        if (!IsOperator(token) && !IsInternalMetaChar(token[0]))
        {
            if (!ToDouble(token, refValue[token])) return 0;
        }
    }

    return GenValue(refValue, err);
}

int main()
{
    std::string err;
    MathCalculator calc;

    double d = calc.GenValue("1 + 2 + 3 * 2", err);
    assert(err.empty());
    assert(fabs(d - 9) < 1e-10);

    d = calc.GenValue("-3 + 2*2 + 3 * 2", err);
    assert(err.empty());
    assert(fabs(d - 7) < 1e-10);

    d = calc.GenValue("-3 + 2*(-2 + 3) * 2", err);
    assert(err.empty());
    assert(fabs(d - 1) < 1e-10);

    return 0;
}

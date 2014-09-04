#ifndef __MATH_CALC_H_
#define __MATH_CALC_H_

#include <map>
#include <set>
#include <string>
#include <vector>

class MathCalculator
{
    public:

        MathCalculator();

        explicit MathCalculator(const std::set<std::string>* ref);

        // to parse the arithmetic expression to a postfix form, but not generate the value.
        // eg, turn "a + b + 3 + c" into "ab+3+c+", to generate the corresponding value, call GenValue()
        // and pass in the referenced value in a map.
        bool ParseExpression(const std::string& exp, std::string& err);

        // generate the value for the arithmetic expression on given reference values.
        // make sure to call ParseExpression() first.
        double GenValue(const std::map<std::string, double>& ref, std::string& err) const;

        // calculate the value of a literal arithmetic expression directly, like, calculate "1 + 2 + 3" -> 6
        double GenValue(const std::string& exp, std::string& err);

        const std::vector<std::string>& GetTokens() const { return m_tokens; }

    public:

        enum OpType
        {
            op_id_none,
            op_id_add,
            op_id_sub,
            op_id_mul,
            op_id_div,
            op_id_neg,
        };

        struct TokenItem
        {
            OpType op;
            std::string token;
        };

        // meta char, include: a) operator, b) reversed char, like, parenthesis.
        static bool IsMetaChar(char c);

        // currently, this refers to parenthesis.
        static bool IsInternalMetaChar(char c);

        // decide a token is an operator or not.
        static int IsOperator(const std::string& c);

        static bool IsOperatorChar(char c);

        static std::string ExtractToken(const char*& s, const char* e);

        const std::vector<TokenItem>& GetPostfixExp() const { return m_postFixExp; }

    private:

        static bool IsValidBinOperator(const std::vector<std::string>& tokens, int i);
        static bool IsValidUnaryOperator(const std::vector<std::string>& tokens, int i);
        static bool IsValidOperand(const std::vector<std::string>& tokens, int i);

        bool GenPostfixExp(const std::vector<std::string>& tokens,
                std::vector<TokenItem>& postExp, std::string& err);

    private:

        std::vector<std::string> m_tokens;
        std::vector<TokenItem> m_postFixExp;
        const std::set<std::string>* m_reference;
};

#endif

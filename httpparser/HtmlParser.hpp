#ifndef HTTP_PARSER_HPP_INCLUDED
#define HTTP_PARSER_HPP_INCLUDED

#include <vector>
#include <string>

#include "FastCType.hpp"

struct HtmlAttribute
{
	std::string Name;
	std::string Value;
};

struct HtmlElement
{
	std::string Name;
	std::vector<HtmlAttribute> Attributes;
	std::string Content;
	std::string Code;
};

struct IHtmlParserEvents
{
    virtual ~IHtmlParserEvents(){}
	virtual void OnDocumentStart() {}
	virtual void OnDocumentEnd() {}

	virtual void OnStartTag(const char* Name, size_t Length) = 0;
	virtual void OnStartTagClose(const char* p) = 0;
	virtual void OnEndTag(const char* Name, size_t Length) = 0;

	virtual void OnAttribute( const char* Name, size_t NameLength, const char* Value, size_t ValueLength) {}

	virtual void OnPlainText(const char* Name, size_t Length) {}
	virtual void OnScript(const char* Name, size_t Length) {}
	virtual void OnComment(const char* Name, size_t Length) {}
	virtual void OnDeclaration(const char* Value, size_t Length) {}
	virtual void OnInstruction(const char* Value, size_t Length) {}
	
	virtual void OnError(const char* Name, size_t Length) {}
};

class HtmlParser
{
	typedef HtmlParser ThisType;
	typedef bool (ThisType::*StateType)();
public:
	HtmlParser(IHtmlParserEvents& EventsSink) : 
		m_CurrentState(&ThisType::PlainState),
		m_Aborted(false),
		m_InScript(false),
		m_EventsSink(&EventsSink)
	{
	}
	
	virtual ~HtmlParser(){}
	
	size_t Parse(const void* Document, size_t DocumentSize);
	void Abort() { m_Aborted = true; }
	bool Aborted() const { return m_Aborted; }

private:
	bool PlainState();
	bool LeftBracketState();
	bool ElementBeginState();
	bool ElementEndState();
	bool AttributeState();
	bool CommentState();
	bool DeclarationState();
	bool InstructionState();
	bool ScriptState();
	bool ErrorState();

	bool RunCurrentState();

private:
	void SwitchTo(StateType state);

private:
	// used as functions, so without 'm_' prefix
	static const CharSet IsAttributeNameChar;
	static const CharSet IsElementLeadingChar;
	static const CharSet IsElementChar;

private:
	const char* m_DocumentBegin;
	const char* m_DocumentEnd;
	const char* m_Cursor;

	StateType m_CurrentState;
	bool m_Aborted;
	bool m_InScript;

	// for joining multiple line quoted attribute value 
	std::string m_QuotedValue;
	IHtmlParserEvents* m_EventsSink;
};

#endif//HTTP_PARSER_HPP_INCLUDED

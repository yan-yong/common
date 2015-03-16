#include "HtmlParser.hpp"

#include <string>
#include <cassert>

#include <ctype.h>

namespace
{
const CharSet& IsAlpha = GetAlphaSet();
const CharSet& IsUpper = GetUpperSet();
const CharSet& IsLower = GetLowerSet();
const CharSet& IsDigit = GetDigitSet();
const CharSet& IsAlphaNum = GetAlphaNumSet();
const CharSet& IsPrint = GetPrintSet();
const CharSet& IsSpace = GetSpaceSet();
const CharSet& IsHex = GetHexSet();
}

const CharSet HtmlParser::IsAttributeNameChar("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_:");
const CharSet HtmlParser::IsElementLeadingChar("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.:_-");
const CharSet HtmlParser::IsElementChar("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-:");

void HtmlParser::SwitchTo(StateType state)
{
	m_CurrentState = state;
}

bool HtmlParser::PlainState()
{
	const char* p;

	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '<')
		{
			SwitchTo(&ThisType::LeftBracketState);
			return true;
		}
		else
		{
			// Skip leading white spaces
			if (!IsSpace(*p))
			{
				const char* Begin = p;
				const char* last_nonspace = p;
				while ((p = m_Cursor++) < m_DocumentEnd)
				{
					if (*p == '<')
					{
						m_EventsSink->OnPlainText(Begin, last_nonspace + 1 - Begin);
						SwitchTo(&ThisType::LeftBracketState);
						return true;
					}
					else
					{
						if (!IsSpace(*p))
							last_nonspace = p;
					}
				}
				m_EventsSink->OnPlainText(Begin, last_nonspace + 1 - Begin);
				return false;
			}
		}
	}

	return false;
}

bool HtmlParser::LeftBracketState()
{
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (IsElementLeadingChar(*p))
		{
			SwitchTo(&ThisType::ElementBeginState);
			return true;
		}
		else
		{
			switch (*p)
			{
			case '/':
				SwitchTo(&ThisType::ElementEndState);
				return true;
				break;
			case '!':
				if (p + 2 < m_DocumentEnd && p[1] == '-' && p[2] == '-')
				{
					++m_Cursor;
					SwitchTo(&ThisType::CommentState);
				}
				else if (p + 1 < m_DocumentEnd && IsAlpha(p[1]))
				{
					SwitchTo(&ThisType::DeclarationState);
				}
				else
				{
					SwitchTo(&ThisType::ErrorState);
				}
				return true;
				break;
			case '?':
				SwitchTo(&ThisType::InstructionState);
				return true;
				break;
			default:
				// error
				break;
			}
		}
	}
	return false;
}

bool HtmlParser::ElementBeginState()
{
	const char* Begin = m_Cursor - 1;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (!IsElementChar(*p))
		{
			m_InScript = 
				p - Begin == 6 &&
				(Begin[0]=='S' || Begin[0]=='s') &&
				(Begin[1]=='C' || Begin[1]=='c') &&
				(Begin[2]=='R' || Begin[2]=='r') &&
				(Begin[3]=='I' || Begin[3]=='i') &&
				(Begin[4]=='P' || Begin[4]=='p') &&
				(Begin[5]=='T' || Begin[5]=='t');

			m_EventsSink->OnStartTag(Begin, p - Begin);
			if (IsSpace(*p))
			{
				SwitchTo(&ThisType::AttributeState);
			}
			else if (*p == '>')
			{
				m_EventsSink->OnStartTagClose(p);
				if (m_InScript)
					SwitchTo(&ThisType::ScriptState);
				else
					SwitchTo(&ThisType::PlainState);
			}
			else
			{
				// put error handle here
				return true;
			}
			return true;
		}
	}
	return false;
}

bool HtmlParser::ScriptState()
{
	const char* Begin = m_Cursor;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '>')
		{
			if (p - Begin > 7)
			{
				const char* e = p - 8; // "</SCRIPT>";
				if (e[0] == '<' && e[1] == '/' &&
					(e[2]=='S' || e[2]=='s') &&
					(e[3]=='C' || e[3]=='c') &&
					(e[4]=='R' || e[4]=='r') &&
					(e[5]=='I' || e[5]=='i') &&
					(e[6]=='P' || e[6]=='p') &&
					(e[7]=='T' || e[7]=='t'))
				{
					if (Begin != e)
						m_EventsSink->OnScript(Begin, e - Begin);
					m_EventsSink->OnEndTag(e + 1, p - (e + 1));
					SwitchTo(&ThisType::PlainState);
					return true;
				}
			}
		}
	}
	return false;
}

bool HtmlParser::ElementEndState()
{
	for (; IsSpace(*m_Cursor); ++m_Cursor)
	{
		if (m_Cursor >= m_DocumentEnd)
		   return false;
	}

	const char* Begin = m_Cursor;
	const char* End = Begin;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '>')
		{
			m_EventsSink->OnEndTag(Begin, End - Begin);
			SwitchTo(&ThisType::PlainState);
			return true;
		}
		else if (!IsSpace(*p))
		{
			End = p + 1;
		}
	}
	return false;
}

bool HtmlParser::AttributeState()
{
	const char* Name;
	size_t NameLength;
	const char* ValueBegin;
	//const char* ValueEnd;
	const char* p;
	char QuotChar;
	for (;;)
	{
	State_Init:
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			if (IsAttributeNameChar(*p))
			{
				Name = p;
				goto State_Name;
			}
			else if (*p == '>')
			{
				m_EventsSink->OnStartTagClose(p);
				SwitchTo(m_InScript ? &ThisType::ScriptState : &ThisType::PlainState);
				return true;
			}
			else if (*p == '/')
			{
				SwitchTo(&ThisType::ElementEndState);
				m_InScript = false;
				return true;
			}
		}
		break;
	State_Name:
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			if (!IsAttributeNameChar(*p))
			{
				switch(*p)
				{
				case '=':
					NameLength = p - Name;
					goto State_Colon;
				case '>':
					m_EventsSink->OnAttribute(Name, p - Name, NULL, 0);
					m_EventsSink->OnStartTagClose(p);
					SwitchTo(m_InScript ? &ThisType::ScriptState : &ThisType::PlainState);
					return true;
				case '/':
					m_EventsSink->OnAttribute(Name, p - Name, NULL, 0);
					SwitchTo(&ThisType::ElementEndState);
					m_InScript = false;
					return true;
				default:
					if (IsSpace(*p))
					{
						NameLength = p - Name;
						goto State_WantColon;
					}
					else
					{
						; // error
					}
				}
			}
		}
		break;
	State_WantColon:
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			if (*p == '=')
			{
				goto State_Colon;
			}
			else if (!IsSpace(*p))
			{
				m_EventsSink->OnAttribute(Name, NameLength, NULL, 0);
				Name = p;
				goto State_Name;
			}
		}
		break;
	State_Colon:
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			switch (*p)
			{
			case '\'':
			case '"':
				QuotChar = *p;
				goto State_QuotedValue;
				break;
			default:
				if (!IsSpace(*p))
				{
					ValueBegin = p;
					goto State_UnquotedValue;
				}
			}
		}
		break;
	State_QuotedValue:
		ValueBegin = m_Cursor;
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			if (*p == QuotChar)
			{
				m_EventsSink->OnAttribute(Name, NameLength, ValueBegin, p - ValueBegin);
				goto State_Init;
			}
			else if (*p=='\r' || *p=='\n')
			{
				m_QuotedValue.assign(ValueBegin, p - ValueBegin);
				goto State_QuotedValueLineContinuation;
			}
		}
		break;
	State_QuotedValueLineContinuation:
		ValueBegin = NULL;
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			if (*p == QuotChar)
			{
				if (ValueBegin)
					m_QuotedValue.append(ValueBegin, p - ValueBegin);
				m_EventsSink->OnAttribute(Name, NameLength, m_QuotedValue.data(), m_QuotedValue.length());
				goto State_Init;
			}
			else if (*p=='\r' || *p=='\n')
			{
				if (ValueBegin)
				{
					m_QuotedValue.append(ValueBegin, p - ValueBegin);
					ValueBegin = NULL;
				}
			}
			else if (!ValueBegin && *p != '\t') // skip leading whitespaces
			{
				ValueBegin = p;
			}
		}
		break;
	State_UnquotedValue:
		while ((p = m_Cursor++) < m_DocumentEnd)
		{
			if (IsSpace(*p))
			{
				m_EventsSink->OnAttribute(Name, NameLength, ValueBegin, p - ValueBegin);
				goto State_Init;
			}
			else if (*p == '>')
			{
				m_EventsSink->OnAttribute(Name, NameLength, ValueBegin, p - ValueBegin);
				m_EventsSink->OnStartTagClose(p);
				SwitchTo(m_InScript ? &ThisType::ScriptState : &ThisType::PlainState);
				return true;
			}
		}
		break;
	}
	return false;
}

bool HtmlParser::CommentState()
{
	const char* Begin = m_Cursor;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '>')
		{
			if (
				(p - Begin > 4 && p[-1] == '-' && p[-2] == '-') ||
				(p - Begin > 5 && p[-1] == '!' && p[-2] == '-' && p[-3] == '-')
				)
			{
				m_EventsSink->OnComment(Begin + 2, p - Begin - 4);
				SwitchTo(&ThisType::PlainState);
				return true;
			}
		}
	}
	return false;
}

bool HtmlParser::InstructionState()
{
	const char* Begin = m_Cursor;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '>' && p - Begin > 2 && p[-1] == '?')
		{
			m_EventsSink->OnInstruction(Begin, p - Begin);
			SwitchTo(&ThisType::PlainState);
			return true;
		}
	}
	return false;
}

bool HtmlParser::DeclarationState()
{
	const char* Begin = m_Cursor;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '>')
		{
			m_EventsSink->OnDeclaration(Begin, p - Begin);
			SwitchTo(&ThisType::PlainState);
			return true;
		}
	}
	return false;
}

bool HtmlParser::ErrorState()
{
	const char* Begin = m_Cursor - 1;
	const char* p;
	while ((p = m_Cursor++) < m_DocumentEnd)
	{
		if (*p == '>')
		{
			m_EventsSink->OnError(Begin, p - Begin);
			SwitchTo(&ThisType::PlainState);
			return true;
		}
	}
	return false;
}

bool HtmlParser::RunCurrentState()
{
	return (this->*m_CurrentState)();
}

size_t HtmlParser::Parse(const void* Document, size_t DocumentSize)
{
	m_DocumentBegin = reinterpret_cast<const char*>(Document);
	m_DocumentEnd = m_DocumentBegin + DocumentSize;
	m_Cursor = m_DocumentBegin;

	m_Aborted = false;
	m_InScript = false;

	m_EventsSink->OnDocumentStart();
	SwitchTo(&ThisType::PlainState);
	while (!m_Aborted && RunCurrentState())
	{
	}
	m_EventsSink->OnDocumentEnd();
	return m_Cursor - m_DocumentBegin;
}

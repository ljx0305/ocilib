#include "ocilib_tests.h"

TEST(ReportedIssuesCApi, Issue216)
{
    ASSERT_TRUE(OCI_Initialize(nullptr, HOME, OCI_ENV_DEFAULT));

    const auto conn = OCI_ConnectionCreate(DBS, USR, PWD, OCI_SESSION_DEFAULT);
    ASSERT_NE(nullptr, conn);

    ASSERT_TRUE(OCI_Immediate(conn, OTEXT("CREATE TABLE ISSUE_216(ID NUMBER, DATEVALUE DATE)")));

    const auto stmt = OCI_StatementCreate(conn);
    ASSERT_NE(nullptr, stmt);

    const int totalRowCount = 54;
    const int bufferSize = 50;

    int offset = 0;
    int actualRowCount = bufferSize;
    
    auto ints = static_cast<int*>(calloc(actualRowCount, sizeof(int)));
    auto dates = static_cast<OCI_Date**>(calloc(actualRowCount, sizeof(OCI_Date*)));

    ASSERT_TRUE(OCI_Prepare(stmt, OTEXT("INSERT INTO ISSUE_216(ID, DATEVALUE) VALUES(:i, :d)")));
    ASSERT_TRUE(OCI_BindArraySetSize(stmt, bufferSize));
    ASSERT_TRUE(OCI_BindArrayOfInts(stmt, OTEXT(":i"), ints, 0));
    ASSERT_TRUE(OCI_BindArrayOfDates(stmt, OTEXT(":d"), dates, 0));

    while (offset < totalRowCount)
    {
        for (int i = 0; i < actualRowCount; i++)
        {
            if (dates[i])
            {
                ASSERT_TRUE(OCI_DateFree(dates[i]));
            }
        }

        actualRowCount = totalRowCount - offset < bufferSize ? totalRowCount - offset : bufferSize;

        for (int i = 0; i < actualRowCount; i++)
        {
            ints[i] = i;
            dates[i] = OCI_DateCreate(NULL);
            ASSERT_NE(nullptr, dates[i]);
            ASSERT_TRUE(OCI_DateSysDate(dates[i]));
        }

        if (actualRowCount < bufferSize)
        {
            ASSERT_TRUE(OCI_BindArraySetSize(stmt, actualRowCount));
        }

        ASSERT_TRUE(OCI_Execute(stmt));
        offset += actualRowCount;
    }

    ASSERT_EQ(totalRowCount, offset);

    for (int i = 0; i < actualRowCount; i++)
    {
        ASSERT_TRUE(OCI_DateFree(dates[i]));
    }

    ASSERT_TRUE(OCI_Immediate(conn, OTEXT("DROP TABLE ISSUE_216")));

    ASSERT_TRUE(OCI_ConnectionFree(conn));
    ASSERT_TRUE(OCI_Cleanup());
}

TEST(ReportedIssuesCApi, Issue222)
{
    ASSERT_TRUE(OCI_Initialize(nullptr, HOME, OCI_ENV_DEFAULT));

    const auto pool = OCI_PoolCreate(DBS, SYS_USR, SYS_PWD, OCI_POOL_SESSION, OCI_SESSION_SYSDBA, 0, 1, 1);
    ASSERT_NE(nullptr, pool);

    const auto conn = OCI_PoolGetConnection(pool, nullptr);
    ASSERT_NE(nullptr, conn);

    ASSERT_TRUE(OCI_ConnectionFree(conn));
    ASSERT_TRUE(OCI_PoolFree(pool));
    ASSERT_TRUE(OCI_Cleanup());
}

TEST(ReportedIssuesCApi, Issue225)
{
    ASSERT_TRUE(OCI_Initialize(nullptr, HOME, OCI_ENV_DEFAULT));

    const auto conn = OCI_ConnectionCreate(DBS, USR, PWD, OCI_SESSION_DEFAULT);
    ASSERT_NE(nullptr, conn);

    ASSERT_TRUE(OCI_ServerEnableOutput(conn, 32000, 5, 255));

    const auto stmt = OCI_StatementCreate(conn);
    ASSERT_NE(nullptr, stmt);

    ASSERT_TRUE(OCI_Prepare(stmt,
        OTEXT(
            "declare "
            "type val_tab is table of number index by binary_integer;"
            "procedure Test( v IN OUT val_tab ) is "
            "begin "
            "DBMS_OUTPUT.PUT_LINE(v.count); "
            "for i in 1..v.count loop "
            "  v(i) := i+10; "
            "  DBMS_OUTPUT.PUT_LINE('v('||i||') = '||v(i));"
            "end loop; "
            "end Test; "
            "begin "
            " Test(:v);"
            " end;")));

    std::array<OCI_Number*, 3> numbers;

    for (size_t i = 0; i < numbers.size(); i++)
    {
        numbers[i] = OCI_NumberCreate(conn);
        ASSERT_NE(nullptr, numbers[i]);
        ASSERT_TRUE(OCI_NumberFromText(numbers[i], TO_STRING(i+1).data(), nullptr));
    }

    ASSERT_TRUE(OCI_BindArrayOfNumbers(stmt, OTEXT(":v"), numbers.data(), static_cast<unsigned int>(numbers.size())));
    ASSERT_TRUE(OCI_BindSetDirection(OCI_GetBind(stmt, 1), OCI_BDM_IN_OUT));
    ASSERT_TRUE(OCI_Execute(stmt));

    ASSERT_EQ(TO_STRING(numbers.size()), ostring(OCI_ServerGetOutput(conn)));

    for (size_t i = 0; i < numbers.size(); i++)
    {
        otext buffer[100];
        osprintf(buffer, OTEXT("v(%d) = %d"), static_cast<int>(i) + 1, static_cast<int>(i) + 1 + 10);
        ASSERT_EQ(ostring(buffer), ostring(OCI_ServerGetOutput(conn)));
    }

    for (size_t i = 0; i < numbers.size(); i++)
    {
        ostring expected = TO_STRING(i+1+10);
        otext buffer[100];
        ASSERT_NE(0, OCI_NumberToText(numbers[i], NULL, 100, buffer));
        ASSERT_EQ(expected, ostring(buffer));
        ASSERT_TRUE(OCI_NumberFree(numbers[i]));
    }
 
    ASSERT_TRUE(OCI_ConnectionFree(conn));
    ASSERT_TRUE(OCI_Cleanup());
}


#ifdef OCI_CHARSET_ANSI

TEST(ReportedIssuesCApi, Issue238)
{
    ExecDML(OTEXT("create table FetchLobAsString(value clob)"));

    const char* nsl_lang = getenv("NLS_LANG");
    putenv((char*)"NLS_LANG=American_America.UTF8");

    ASSERT_TRUE(OCI_Initialize(nullptr, HOME, OCI_ENV_DEFAULT));

    const auto conn = OCI_ConnectionCreate(DBS, USR, PWD, OCI_SESSION_DEFAULT);
    ASSERT_NE(nullptr, conn);

    const auto stmt = OCI_StatementCreate(conn);
    ASSERT_NE(nullptr, stmt);

    ASSERT_TRUE(OCI_ExecuteStmt(stmt, OTEXT("declare c clob;  begin  c := c||rpad('a', 32767, 'a'); insert into FetchLobAsString values (c); end;")));
    ASSERT_TRUE(OCI_ExecuteStmt(stmt, OTEXT("select value from FetchLobAsString")));

    auto rslt = OCI_GetResultset(stmt);
    ASSERT_NE(nullptr, rslt);

    ASSERT_TRUE(OCI_FetchNext(rslt));

    auto value = OCI_GetString(rslt, 1);
    ASSERT_NE(nullptr, value);
    auto len = strlen(value);
    ASSERT_EQ(32767, len);

    ASSERT_TRUE(OCI_StatementFree(stmt));
    ASSERT_TRUE(OCI_ConnectionFree(conn));
    ASSERT_TRUE(OCI_Cleanup());

    std::string new_nls_lang = "NLS_LANG=";

    if (nsl_lang)
    {
        new_nls_lang += nsl_lang;
    }
    putenv((char*)new_nls_lang.data());

    ExecDML(OTEXT("drop table FetchLobAsString;"));
}

#endif

class ReportedIssues247 : public ::testing::TestWithParam<unsigned int> {};

static std::vector<unsigned int> TimestampTypes{ OCI_TIMESTAMP, OCI_TIMESTAMP_TZ, OCI_TIMESTAMP_LTZ };
static Context contextIssue247NoNullPointerException;

static void AddErrorIssue247NoNullPointerException(OCI_Error* err)
{
    contextIssue247NoNullPointerException.Errs.emplace_back(Error{ OCI_ErrorGetInternalCode(err), OCI_ErrorGetOCICode(err) });
}

TEST_P(ReportedIssues247, NoNullPointerException)
{
    ASSERT_TRUE(OCI_Initialize(AddErrorIssue247NoNullPointerException, HOME, OCI_ENV_DEFAULT));

    const auto tmsp = OCI_TimestampCreate(nullptr, GetParam());
    ASSERT_TRUE(OCI_TimestampSysTimestamp(tmsp));
    ASSERT_TRUE(OCI_TimestampFree(tmsp));

    ASSERT_TRUE(contextIssue247NoNullPointerException.Errs.empty());

    ASSERT_TRUE(OCI_Cleanup());
}

TEST(ReportedIssuesCApi, Issue262)
{
    ExecDML(OTEXT("CREATE TABLE test_urowid(name VARCHAR2(256 CHAR) NOT NULL, CONSTRAINT test_urowid PRIMARY KEY (name) ) ORGANIZATION INDEX"));
    ExecDML(OTEXT("INSERT INTO test_urowid VALUES (LPAD('a', 200, 'a'))"));

    ASSERT_TRUE(OCI_Initialize(nullptr, HOME, OCI_ENV_DEFAULT));

    const auto conn = OCI_ConnectionCreate(DBS, USR, PWD, OCI_SESSION_DEFAULT);
    ASSERT_NE(nullptr, conn);

    const auto stmt = OCI_StatementCreate(conn);
    ASSERT_NE(nullptr, stmt);

    ASSERT_TRUE(OCI_ExecuteStmt(stmt, OTEXT("SELECT NAME, LENGTH(ROWID), ROWID FROM test_urowid")));

    const auto rslt = OCI_GetResultset(stmt);
    ASSERT_NE(nullptr, rslt);
 
    ASSERT_TRUE(OCI_FetchNext(rslt));
    const auto name = OCI_GetString(rslt, 1);
    ASSERT_NE(nullptr, name);
    const auto length = static_cast<size_t>(OCI_GetInt(rslt, 2));
    ASSERT_EQ(true, length > 0);
    const auto rowid = OCI_GetString(rslt, 3);
    ASSERT_NE(nullptr, rowid);

    const auto expected_length = length;
    const auto real_length = ostrlen(rowid);
    ASSERT_EQ(expected_length, real_length);

    ASSERT_TRUE(OCI_ConnectionFree(conn));
    ASSERT_TRUE(OCI_Cleanup());

    ExecDML(OTEXT("DROP TABLE test_urowid"));
}


INSTANTIATE_TEST_CASE_P(ReportedIssuesCApi, ReportedIssues247, ::testing::ValuesIn(TimestampTypes));

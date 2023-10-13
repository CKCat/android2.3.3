<?xml version="1.0" encoding="utf-8"?>
<!-- Copyright (C) 2008 The Android Open Source Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
-->

<!DOCTYPE xsl:stylesheet [ <!ENTITY nbsp "&#160;"> ]>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>

    <xsl:template match="/">

        <html>
            <STYLE type="text/css">
                @import "cts_result.css";
            </STYLE>

            <body>
                <!-- Title of the Report -->
                <DIV id="title">
                    <TABLE>
                        <TR>
                            <TD width="40%" align="left"><img src="logo.gif"></img></TD>
                            <TD width="60%" align="left">
                                <h1>Test Report for <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@build_model"/> -
                                <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@deviceID"/>
                            </h1>
                        </TD>
                    </TR>
                </TABLE>
            </DIV>
            <img src="newrule-green.png" align="left"></img>

            <br></br>
            <br></br>

            <!-- Header with phone and plan information -->
            <DIV id="summary">
                <TABLE width="90%" frame="none">
                    <TR>
                        <TH>Device Information</TH>
                        <TH>Test Summary</TH>
                    </TR>

                    <TR>
                        <TD>
                            <!-- Device information -->
                            <div id="summaryinfo">
                                <TABLE width="75%">
                                    <TR>
                                        <TD class="rowtitle">Build Model</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@build_model"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Build Name</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@buildName"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Device ID</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@deviceID"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Firmware Version</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@buildVersion"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Firmware Build Number</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@buildID"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Build Fingerprint</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@build_fingerprint"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Build ABI</TD>
                                        <TD>
                                            <xsl:value-of
                                              select="TestResult/DeviceInfo/BuildInfo/@build_abi"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Build ABI2</TD>
                                        <TD>
                                            <xsl:value-of
                                              select="TestResult/DeviceInfo/BuildInfo/@build_abi2"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Android Platform Version</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@androidPlatformVersion"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Supported Locales</TD>
                                        <TD>
                                            <xsl:call-template name="formatDelimitedString">
                                                <xsl:with-param name="string" select="TestResult/DeviceInfo/BuildInfo/@locales"/>
                                            </xsl:call-template>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Screen size</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/Screen/@resolution"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Phone number</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/PhoneSubInfo/@subscriberId"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">x dpi</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@Xdpi"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">y dpi</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@Ydpi"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Touch</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@touch"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Navigation</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@navigation"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Keypad</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@keypad"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Network</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@network"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">IMEI</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@imei"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">IMSI</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@imsi"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Open GL ES Version</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/DeviceInfo/BuildInfo/@openGlEsVersion"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Features</TD>
                                        <TD>
                                            <xsl:for-each select="TestResult/DeviceInfo/FeatureInfo/Feature[@type='sdk']">
                                                <xsl:text>[</xsl:text>
                                                <xsl:choose>
                                                    <xsl:when test="@available = 'true'">
                                                        <xsl:text>X</xsl:text>
                                                    </xsl:when>
                                                    <xsl:otherwise>
                                                        <xsl:text>_</xsl:text>
                                                    </xsl:otherwise>
                                                </xsl:choose>
                                                <xsl:text>] </xsl:text>

                                                <xsl:value-of select="@name" />
                                                <br />
                                            </xsl:for-each>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Other Features</TD>
                                        <TD>
                                            <UL>
                                                <xsl:for-each select="TestResult/DeviceInfo/FeatureInfo/Feature[@type='other']">
                                                    <LI><xsl:value-of select="@name" /></LI>
                                                </xsl:for-each>
                                            </UL>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Root Processes</TD>
                                        <TD>
                                            <UL>
                                                <xsl:for-each select="TestResult/DeviceInfo/ProcessInfo/Process[@uid='0']">
                                                    <LI><xsl:value-of select="@name" /></LI>
                                                </xsl:for-each>
                                            </UL>
                                        </TD>
                                    </TR>
                                </TABLE>
                            </div>
                        </TD>

                        <!-- plan information -->
                        <TD>
                            <div id="summaryinfo">
                                <TABLE width="75%">
                                    <TR>
                                        <TD class="rowtitle">CTS version</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/HostInfo/Cts/@version"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Test timeout</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/HostInfo/Cts/IntValue[@name='testStatusTimeoutMs']/@value" /> ms
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Host Info</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/HostInfo/@name"/>
                                            (<xsl:value-of select="TestResult/HostInfo/Os/@name"/> - 
                                              <xsl:value-of select="TestResult/HostInfo/Os/@version"/>)
                                        </TD>
                                    </TR>
                                    <TR><TD><BR></BR></TD><TD></TD></TR>
                                    <TR>
                                        <TD class="rowtitle">Plan name</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/@testPlan"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Profile</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/@profile"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Start time</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/@starttime"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">End time</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/@endtime"/>
                                        </TD>
                                    </TR>

                                    <!-- Test Summary -->
                                    <TR><TD><BR></BR></TD><TD></TD></TR>
                                    <TR>
                                        <TD class="rowtitle">Tests Passed</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/Summary/@pass"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Tests Failed</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/Summary/@failed"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Tests Timed out</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/Summary/@timeout"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Tests Omitted</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/Summary/@omitted"/>
                                        </TD>
                                    </TR>
                                    <TR>
                                        <TD class="rowtitle">Tests Not Executed</TD>
                                        <TD>
                                            <xsl:value-of select="TestResult/Summary/@notExecuted"/>
                                        </TD>
                                    </TR>
                                </TABLE>
                            </div>
                        </TD>
                    </TR>
                </TABLE>
            </DIV>

            <!-- High level summary of test execution -->
            <h2 align="center">Test Summary by Package</h2>
            <DIV id="testsummary">
                <TABLE>
                    <TR>
                        <TH>Test Package</TH>
                        <TH>Passed</TH>
                        <TH>Failed</TH>
                        <TH>Timed Out</TH>
                        <TH>Total Tests</TH>
                    </TR>
                    <xsl:for-each select="TestResult/TestPackage">
                        <TR>
                            <TD>
                                <xsl:variable name="href"><xsl:value-of select="@name"/></xsl:variable>
                                <a href="#{$href}"><xsl:value-of select="@name"/></a>
                            </TD>
                            <TD>
                                <xsl:value-of select="@pass"/>
                            </TD>
                            <TD>
                                <xsl:value-of select="@failed"/>
                            </TD>
                            <TD>
                                <xsl:value-of select="@timeout"/>
                            </TD>
                            <TD>
                                <xsl:value-of select="@total"/>
                            </TD>
                        </TR>
                    </xsl:for-each> <!-- end package -->
                </TABLE>
            </DIV>

            <!-- Details of all the executed tests -->
            <h2 align="center">Detailed Test Report</h2>

            <!-- test package -->
            <DIV id="testdetail">
                <xsl:for-each select="TestResult/TestPackage">
                    <DIV id="none">
                        <TABLE>
                            <TR>
                                <TD class="none" align="left">
                                    <xsl:variable name="href"><xsl:value-of select="@name"/></xsl:variable>
                                    <a name="{$href}">Compatibility Test Package: <xsl:value-of select="@name"/></a>
                                </TD>
                            </TR>
                        </TABLE>
                    </DIV>

                    <TABLE>
                        <TR>
                            <TH width="25%">Test</TH>
                            <TH width="7%">Result</TH>
                            <TH width="68%">Failure Details</TH>
                        </TR>

                        <!-- test case -->
                        <xsl:for-each select="TestCase">

                            <!-- emit a blank row before every test suite name -->
                            <xsl:if test="position()!=1">
                                <TR> <TD class="testcasespacer" colspan="3"></TD> </TR>
                            </xsl:if>


                            <TR>
                                <TD class="testcase" colspan="3">
                                    <xsl:value-of select="@name"/>
                                </TD>
                            </TR>
                            
                            <!-- test -->
                            <xsl:for-each select="Test">
                                <TR>
                                    <TD class="testname"> -- <xsl:value-of select="@name"/></TD>

                                    <!-- test results -->
                                    <xsl:choose>
                                        <xsl:when test="string(@KnownFailure)">
                                            <!-- "pass" indicates the that test actually passed (results have been inverted already) -->
                                            <xsl:if test="@result='pass'">
                                                <TD class="pass">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        known problem
                                                    </div>
                                                </TD>
                                                <TD class="failuredetails"></TD>
                                            </xsl:if>

                                            <!-- "fail" indicates that a known failure actually passed (results have been inverted already) -->
                                            <xsl:if test="@result='fail'">
                                                <TD class="failed">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        <xsl:value-of select="@result"/>
                                                    </div>
                                                </TD>
                                               <TD class="failuredetails">
                                                    <div id="details">
                                                        A test that was a known failure actually passed. Please check.
                                                    </div>
                                               </TD>
                                            </xsl:if>
                                        </xsl:when>

                                        <xsl:otherwise>
                                            <xsl:if test="@result='pass'">
                                                <TD class="pass">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        <xsl:value-of select="@result"/>
                                                    </div>
                                                </TD>
                                                <TD class="failuredetails"></TD>
                                            </xsl:if>

                                            <xsl:if test="@result='fail'">
                                                <TD class="failed">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        <xsl:value-of select="@result"/>
                                                    </div>
                                                </TD>
                                                <TD class="failuredetails">
                                                    <div id="details">
                                                        <xsl:value-of select="FailedScene/@message"/>
                                                    </div>
                                                </TD>
                                            </xsl:if>

                                            <xsl:if test="@result='timeout'">
                                                <TD class="timeout">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        <xsl:value-of select="@result"/>
                                                    </div>
                                                <TD class="failuredetails"></TD>
                                                </TD>
                                            </xsl:if>

                                            <xsl:if test="@result='omitted'">
                                                <TD class="omitted">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        <xsl:value-of select="@result"/>
                                                    </div>
                                                </TD>
                                                <TD class="failuredetails"></TD>
                                            </xsl:if>

                                            <xsl:if test="@result='notExecuted'">
                                                <TD class="notExecuted">
                                                    <div style="text-align: center; margin-left:auto; margin-right:auto;">
                                                        <xsl:value-of select="@result"/>
                                                    </div>
                                                </TD>
                                                <TD class="failuredetails"></TD>
                                            </xsl:if>
                                        </xsl:otherwise>
                                    </xsl:choose>
                                </TR> <!-- finished with a row -->
                            </xsl:for-each> <!-- end test -->
                        </xsl:for-each> <!-- end test case -->
                    </TABLE>
                </xsl:for-each> <!-- end test package -->
            </DIV>
            </body>
        </html>
    </xsl:template>

    <!-- Take a delimited string and insert line breaks after a some number of elements. --> 
    <xsl:template name="formatDelimitedString">
        <xsl:param name="string" />
        <xsl:param name="numTokensPerRow" select="10" />
        <xsl:param name="tokenIndex" select="1" />
        <xsl:if test="$string">
            <!-- Requires the last element to also have a delimiter after it. -->
            <xsl:variable name="token" select="substring-before($string, ';')" />
            <xsl:value-of select="$token" />
            <xsl:text>&#160;</xsl:text>
          
            <xsl:if test="$tokenIndex mod $numTokensPerRow = 0">
                <br />
            </xsl:if>

            <xsl:call-template name="formatDelimitedString">
                <xsl:with-param name="string" select="substring-after($string, ';')" />
                <xsl:with-param name="numTokensPerRow" select="$numTokensPerRow" />
                <xsl:with-param name="tokenIndex" select="$tokenIndex + 1" />
            </xsl:call-template>
        </xsl:if>
    </xsl:template>

</xsl:stylesheet>

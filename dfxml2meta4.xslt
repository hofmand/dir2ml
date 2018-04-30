<?xml version="1.0"?>
<!--
dfxml2meta4.xslt - a stylesheet to convert a Digital Forensics XML file to
Metalinks (RFC5854) .meta4.

Using the following hashdeep command to create the DFXML file:

	hashdeep64 -r -j0 -c sha256 -d C:\MyFilesDir > MyHash.dfxml
	
Then the following SAXON command to convert the DFXML file to .meta4:

	java -jar saxon9he.jar -s:MyHash.dfxml -xsl:dfxml2meta4.xslt -o:MyHash.meta4 baseDirectory="C:\MyFilesDir" baseUrl="http://www.example.com/"
	
Compare it with the output of dir2ml (removing the parentheses first):

	dir2ml (-)-file-url (-)-sparse-output (-)-hash-type sha256 (-)-directory "C:\MyFilesDir" (-)-output MyHash.meta4
-->

<xsl:stylesheet version="2.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:nsi="http://www.w3.org/2001/XMLSchema-instance"
	xpath-default-namespace="http://www.forensicswiki.org/wiki/Category:Digital_Forensics_XML">

	<xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes" />

	<xsl:param name="baseDirectory" />
	<xsl:param name="baseUrl" />

	<xsl:template name="encodeUrl">
		<xsl:param name="url" />
		<xsl:value-of select="replace(replace(replace(replace(replace(replace(replace(translate($url,'\','/'),' ','%20'),',','%2C'),'\(','%28'),'\)','%29'),'\+','%2B'),'@','%40'),'#','%23')"/>
	</xsl:template>

	<xsl:template match="/dfxml">
		<xsl:text>&#xA;</xsl:text>
		<xsl:comment>
			<xsl:text>&#xA;</xsl:text>
			<xsl:text>Arguments used:</xsl:text>
			<xsl:text>&#xA;</xsl:text>
			<xsl:text>&#x9;</xsl:text>
			<xsl:if test="$baseDirectory != ''">
				<xsl:text>baseDirectory=</xsl:text>
				<xsl:value-of select="$baseDirectory"/>
				<xsl:text> </xsl:text>
			</xsl:if>
			<xsl:if test="$baseUrl != ''">
				<xsl:text>baseUrl=</xsl:text>
				<xsl:value-of select="$baseUrl"/>
			</xsl:if>
			<xsl:text>&#xA;</xsl:text>
			<xsl:text># of input files: </xsl:text>
			<xsl:value-of select="format-number(count(fileobject), '###,###')"/>
			<xsl:text>&#xA;</xsl:text>
			<xsl:text># of input bytes: </xsl:text>
			<xsl:value-of select="format-number(sum(fileobject/filesize), '###,###')"/>
			<xsl:text>&#xA;</xsl:text>
		</xsl:comment>
		<xsl:text>&#xA;</xsl:text>
		<metalink xmlns="urn:ietf:params:xml:ns:metalink" xmlns:nsi="http://www.w3.org/2001/XMLSchema-instance" noNamespaceSchemaLocation="metalink4.xsd">
			<generator>dfxml2meta4.xslt/0.1.0</generator>
			<xsl:for-each select="fileobject">
				<xsl:variable name="relativePath" select="translate(substring-after(substring-after(filename,$baseDirectory),'\'),'\','/')" />
				<xsl:text>&#xA;</xsl:text>
				<file>
					<xsl:attribute name="name">
						<xsl:value-of select="$relativePath"/>
					</xsl:attribute>
					<xsl:text>&#xA;</xsl:text>
					<size>
						<xsl:value-of select="filesize"/>
					</size>
					<xsl:for-each select="hashdigest">
						<hash>
							<xsl:attribute name="type">
								<xsl:choose>
									<xsl:when test="@type='MD5'">
										<xsl:text>md5</xsl:text>
									</xsl:when>
									<xsl:when test="@type='SHA1'">
										<xsl:text>sha-1</xsl:text>
									</xsl:when>
									<xsl:when test="@type='SHA256'">
										<xsl:text>sha-256</xsl:text>
									</xsl:when>
									<xsl:otherwise>
										<xsl:value-of select="@type"/>
									</xsl:otherwise>
								</xsl:choose>
							</xsl:attribute>
							<xsl:value-of select="."/>
						</hash>
					</xsl:for-each>
					<url>
						<xsl:text>file:///</xsl:text>
						<xsl:call-template name="encodeUrl">
							<xsl:with-param name="url" select="filename" />
						</xsl:call-template>
					</url>
					<xsl:if test="$baseUrl">
						<url>
							<xsl:call-template name="encodeUrl">
								<xsl:with-param name="url">
							<xsl:value-of select="$baseUrl" />
							<xsl:value-of select="$relativePath" />
								</xsl:with-param>
							</xsl:call-template>
						</url>
					</xsl:if>
				<xsl:text>&#xA;</xsl:text>
			</file>
			</xsl:for-each>
		</metalink>
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>

</xsl:stylesheet>
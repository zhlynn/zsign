#include "common.h"
#include "base64.h"
#include "openssl.h"
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <openssl/pkcs12.h>
#include <openssl/conf.h>

const char* ZSignAsset::s_szAppleDevCACert = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEIjCCAwqgAwIBAgIIAd68xDltoBAwDQYJKoZIhvcNAQEFBQAwYjELMAkGA1UE\n"
"BhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xJjAkBgNVBAsTHUFwcGxlIENlcnRp\n"
"ZmljYXRpb24gQXV0aG9yaXR5MRYwFAYDVQQDEw1BcHBsZSBSb290IENBMB4XDTEz\n"
"MDIwNzIxNDg0N1oXDTIzMDIwNzIxNDg0N1owgZYxCzAJBgNVBAYTAlVTMRMwEQYD\n"
"VQQKDApBcHBsZSBJbmMuMSwwKgYDVQQLDCNBcHBsZSBXb3JsZHdpZGUgRGV2ZWxv\n"
"cGVyIFJlbGF0aW9uczFEMEIGA1UEAww7QXBwbGUgV29ybGR3aWRlIERldmVsb3Bl\n"
"ciBSZWxhdGlvbnMgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggEiMA0GCSqGSIb3\n"
"DQEBAQUAA4IBDwAwggEKAoIBAQDKOFSmy1aqyCQ5SOmM7uxfuH8mkbw0U3rOfGOA\n"
"YXdkXqUHI7Y5/lAtFVZYcC1+xG7BSoU+L/DehBqhV8mvexj/avoVEkkVCBmsqtsq\n"
"Mu2WY2hSFT2Miuy/axiV4AOsAX2XBWfODoWVN2rtCbauZ81RZJ/GXNG8V25nNYB2\n"
"NqSHgW44j9grFU57Jdhav06DwY3Sk9UacbVgnJ0zTlX5ElgMhrgWDcHld0WNUEi6\n"
"Ky3klIXh6MSdxmilsKP8Z35wugJZS3dCkTm59c3hTO/AO0iMpuUhXf1qarunFjVg\n"
"0uat80YpyejDi+l5wGphZxWy8P3laLxiX27Pmd3vG2P+kmWrAgMBAAGjgaYwgaMw\n"
"HQYDVR0OBBYEFIgnFwmpthhgi+zruvZHWcVSVKO3MA8GA1UdEwEB/wQFMAMBAf8w\n"
"HwYDVR0jBBgwFoAUK9BpR5R2Cf70a40uQKb3R01/CF4wLgYDVR0fBCcwJTAjoCGg\n"
"H4YdaHR0cDovL2NybC5hcHBsZS5jb20vcm9vdC5jcmwwDgYDVR0PAQH/BAQDAgGG\n"
"MBAGCiqGSIb3Y2QGAgEEAgUAMA0GCSqGSIb3DQEBBQUAA4IBAQBPz+9Zviz1smwv\n"
"j+4ThzLoBTWobot9yWkMudkXvHcs1Gfi/ZptOllc34MBvbKuKmFysa/Nw0Uwj6OD\n"
"Dc4dR7Txk4qjdJukw5hyhzs+r0ULklS5MruQGFNrCk4QttkdUGwhgAqJTleMa1s8\n"
"Pab93vcNIx0LSiaHP7qRkkykGRIZbVf1eliHe2iK5IaMSuviSRSqpd1VAKmuu0sw\n"
"ruGgsbwpgOYJd+W+NKIByn/c4grmO7i77LpilfMFY0GCzQ87HUyVpNur+cmV6U/k\n"
"TecmmYHpvPm0KdIBembhLoz2IYrF+Hjhga6/05Cdqa3zr/04GpZnMBxRpVzscYqC\n"
"tGwPDBUf\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG3 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEUTCCAzmgAwIBAgIQfK9pCiW3Of57m0R6wXjF7jANBgkqhkiG9w0BAQsFADBi\n"
"MQswCQYDVQQGEwJVUzETMBEGA1UEChMKQXBwbGUgSW5jLjEmMCQGA1UECxMdQXBw\n"
"bGUgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkxFjAUBgNVBAMTDUFwcGxlIFJvb3Qg\n"
"Q0EwHhcNMjAwMjE5MTgxMzQ3WhcNMzAwMjIwMDAwMDAwWjB1MUQwQgYDVQQDDDtB\n"
"cHBsZSBXb3JsZHdpZGUgRGV2ZWxvcGVyIFJlbGF0aW9ucyBDZXJ0aWZpY2F0aW9u\n"
"IEF1dGhvcml0eTELMAkGA1UECwwCRzMxEzARBgNVBAoMCkFwcGxlIEluYy4xCzAJ\n"
"BgNVBAYTAlVTMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2PWJ/KhZ\n"
"C4fHTJEuLVaQ03gdpDDppUjvC0O/LYT7JF1FG+XrWTYSXFRknmxiLbTGl8rMPPbW\n"
"BpH85QKmHGq0edVny6zpPwcR4YS8Rx1mjjmi6LRJ7TrS4RBgeo6TjMrA2gzAg9Dj\n"
"+ZHWp4zIwXPirkbRYp2SqJBgN31ols2N4Pyb+ni743uvLRfdW/6AWSN1F7gSwe0b\n"
"5TTO/iK1nkmw5VW/j4SiPKi6xYaVFuQAyZ8D0MyzOhZ71gVcnetHrg21LYwOaU1A\n"
"0EtMOwSejSGxrC5DVDDOwYqGlJhL32oNP/77HK6XF8J4CjDgXx9UO0m3JQAaN4LS\n"
"VpelUkl8YDib7wIDAQABo4HvMIHsMBIGA1UdEwEB/wQIMAYBAf8CAQAwHwYDVR0j\n"
"BBgwFoAUK9BpR5R2Cf70a40uQKb3R01/CF4wRAYIKwYBBQUHAQEEODA2MDQGCCsG\n"
"AQUFBzABhihodHRwOi8vb2NzcC5hcHBsZS5jb20vb2NzcDAzLWFwcGxlcm9vdGNh\n"
"MC4GA1UdHwQnMCUwI6AhoB+GHWh0dHA6Ly9jcmwuYXBwbGUuY29tL3Jvb3QuY3Js\n"
"MB0GA1UdDgQWBBQJ/sAVkPmvZAqSErkmKGMMl+ynsjAOBgNVHQ8BAf8EBAMCAQYw\n"
"EAYKKoZIhvdjZAYCAQQCBQAwDQYJKoZIhvcNAQELBQADggEBAK1lE+j24IF3RAJH\n"
"Qr5fpTkg6mKp/cWQyXMT1Z6b0KoPjY3L7QHPbChAW8dVJEH4/M/BtSPp3Ozxb8qA\n"
"HXfCxGFJJWevD8o5Ja3T43rMMygNDi6hV0Bz+uZcrgZRKe3jhQxPYdwyFot30ETK\n"
"XXIDMUacrptAGvr04NM++i+MZp+XxFRZ79JI9AeZSWBZGcfdlNHAwWx/eCHvDOs7\n"
"bJmCS1JgOLU5gm3sUjFTvg+RTElJdI+mUcuER04ddSduvfnSXPN/wmwLCTbiZOTC\n"
"NwMUGdXqapSqqdv+9poIZ4vvK7iqF0mDr8/LvOnP6pVxsLRFoszlh6oKw0E6eVza\n"
"UDSdlTs=\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG2 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIC9zCCAnygAwIBAgIIb+/Y9emjp+4wCgYIKoZIzj0EAwIwZzEbMBkGA1UEAwwS\n"
"QXBwbGUgUm9vdCBDQSAtIEczMSYwJAYDVQQLDB1BcHBsZSBDZXJ0aWZpY2F0aW9u\n"
"IEF1dGhvcml0eTETMBEGA1UECgwKQXBwbGUgSW5jLjELMAkGA1UEBhMCVVMwHhcN\n"
"MTQwNTA2MjM0MzI0WhcNMjkwNTA2MjM0MzI0WjCBgDE0MDIGA1UEAwwrQXBwbGUg\n"
"V29ybGR3aWRlIERldmVsb3BlciBSZWxhdGlvbnMgQ0EgLSBHMjEmMCQGA1UECwwd\n"
"QXBwbGUgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkxEzARBgNVBAoMCkFwcGxlIElu\n"
"Yy4xCzAJBgNVBAYTAlVTMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE3fC3BkvP\n"
"3XMEE8RDiQOTgPte9nStQmFSWAImUxnIYyIHCVJhysTZV+9tJmiLdJGMxPmAaCj8\n"
"CWjwENrp0C7JGqOB9zCB9DBGBggrBgEFBQcBAQQ6MDgwNgYIKwYBBQUHMAGGKmh0\n"
"dHA6Ly9vY3NwLmFwcGxlLmNvbS9vY3NwMDQtYXBwbGVyb290Y2FnMzAdBgNVHQ4E\n"
"FgQUhLaEzDqGYnIWWZToGqO9SN863wswDwYDVR0TAQH/BAUwAwEB/zAfBgNVHSME\n"
"GDAWgBS7sN6hWDOImqSKmd6+veuv2sskqzA3BgNVHR8EMDAuMCygKqAohiZodHRw\n"
"Oi8vY3JsLmFwcGxlLmNvbS9hcHBsZXJvb3RjYWczLmNybDAOBgNVHQ8BAf8EBAMC\n"
"AQYwEAYKKoZIhvdjZAYCDwQCBQAwCgYIKoZIzj0EAwIDaQAwZgIxANmxxzHGI/ZP\n"
"TdDZR8V9GGkRh3En02it4Jtlmr5s3z9GppAJvm6hOyywUYlBPIfSvwIxAPxkUolL\n"
"PF2/axzCiZgvcq61m6oaCyNUd1ToFUOixRLal1BzfF7QbrJcYlDXUfE6Wg==\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG4 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEVTCCAz2gAwIBAgIUE9x3lVJx5T3GMujM/+Uh88zFztIwDQYJKoZIhvcNAQEL\n"
"BQAwYjELMAkGA1UEBhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xJjAkBgNVBAsT\n"
"HUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MRYwFAYDVQQDEw1BcHBsZSBS\n"
"b290IENBMB4XDTIwMTIxNjE5MzYwNFoXDTMwMTIxMDAwMDAwMFowdTFEMEIGA1UE\n"
"Aww7QXBwbGUgV29ybGR3aWRlIERldmVsb3BlciBSZWxhdGlvbnMgQ2VydGlmaWNh\n"
"dGlvbiBBdXRob3JpdHkxCzAJBgNVBAsMAkc0MRMwEQYDVQQKDApBcHBsZSBJbmMu\n"
"MQswCQYDVQQGEwJVUzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANAf\n"
"eKp6JzKwRl/nF3bYoJ0OKY6tPTKlxGs3yeRBkWq3eXFdDDQEYHX3rkOPR8SGHgjo\n"
"v9Y5Ui8eZ/xx8YJtPH4GUnadLLzVQ+mxtLxAOnhRXVGhJeG+bJGdayFZGEHVD41t\n"
"QSo5SiHgkJ9OE0/QjJoyuNdqkh4laqQyziIZhQVg3AJK8lrrd3kCfcCXVGySjnYB\n"
"5kaP5eYq+6KwrRitbTOFOCOL6oqW7Z+uZk+jDEAnbZXQYojZQykn/e2kv1MukBVl\n"
"PNkuYmQzHWxq3Y4hqqRfFcYw7V/mjDaSlLfcOQIA+2SM1AyB8j/VNJeHdSbCb64D\n"
"YyEMe9QbsWLFApy9/a8CAwEAAaOB7zCB7DASBgNVHRMBAf8ECDAGAQH/AgEAMB8G\n"
"A1UdIwQYMBaAFCvQaUeUdgn+9GuNLkCm90dNfwheMEQGCCsGAQUFBwEBBDgwNjA0\n"
"BggrBgEFBQcwAYYoaHR0cDovL29jc3AuYXBwbGUuY29tL29jc3AwMy1hcHBsZXJv\n"
"b3RjYTAuBgNVHR8EJzAlMCOgIaAfhh1odHRwOi8vY3JsLmFwcGxlLmNvbS9yb290\n"
"LmNybDAdBgNVHQ4EFgQUW9n6HeeaGgujmXYiUIY+kchbd6gwDgYDVR0PAQH/BAQD\n"
"AgEGMBAGCiqGSIb3Y2QGAgEEAgUAMA0GCSqGSIb3DQEBCwUAA4IBAQA/Vj2e5bbD\n"
"eeZFIGi9v3OLLBKeAuOugCKMBB7DUshwgKj7zqew1UJEggOCTwb8O0kU+9h0UoWv\n"
"p50h5wESA5/NQFjQAde/MoMrU1goPO6cn1R2PWQnxn6NHThNLa6B5rmluJyJlPef\n"
"x4elUWY0GzlxOSTjh2fvpbFoe4zuPfeutnvi0v/fYcZqdUmVIkSoBPyUuAsuORFJ\n"
"EtHlgepZAE9bPFo22noicwkJac3AfOriJP6YRLj477JxPxpd1F1+M02cHSS+APCQ\n"
"A1iZQT0xWmJArzmoUUOSqwSonMJNsUvSq3xKX+udO7xPiEAGE/+QF4oIRynoYpgp\n"
"pU8RBWk6z/Kf\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG5 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEVTCCAz2gAwIBAgIUO36ACu7TAqHm7NuX2cqsKJzxaZQwDQYJKoZIhvcNAQEL\n"
"BQAwYjELMAkGA1UEBhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xJjAkBgNVBAsT\n"
"HUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MRYwFAYDVQQDEw1BcHBsZSBS\n"
"b290IENBMB4XDTIwMTIxNjE5Mzg1NloXDTMwMTIxMDAwMDAwMFowdTFEMEIGA1UE\n"
"Aww7QXBwbGUgV29ybGR3aWRlIERldmVsb3BlciBSZWxhdGlvbnMgQ2VydGlmaWNh\n"
"dGlvbiBBdXRob3JpdHkxCzAJBgNVBAsMAkc1MRMwEQYDVQQKDApBcHBsZSBJbmMu\n"
"MQswCQYDVQQGEwJVUzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAJ9d\n"
"2h/7+rzQSyI8x9Ym+hf39J8ePmQRZprvXr6rNL2qLCFu1h6UIYUsdMEOEGGqPGNK\n"
"fkrjyHXWz8KcCEh7arkpsclm/ciKFtGyBDyCuoBs4v8Kcuus/jtvSL6eixFNlX2y\n"
"e5AvAhxO/Em+12+1T754xtress3J2WYRO1rpCUVziVDUTuJoBX7adZxLAa7a489t\n"
"dE3eU9DVGjiCOtCd410pe7GB6iknC/tgfIYS+/BiTwbnTNEf2W2e7XPaeCENnXDZ\n"
"RleQX2eEwXN3CqhiYraucIa7dSOJrXn25qTU/YMmMgo7JJJbIKGc0S+AGJvdPAvn\n"
"tf3sgFcPF54/K4cnu/cCAwEAAaOB7zCB7DASBgNVHRMBAf8ECDAGAQH/AgEAMB8G\n"
"A1UdIwQYMBaAFCvQaUeUdgn+9GuNLkCm90dNfwheMEQGCCsGAQUFBwEBBDgwNjA0\n"
"BggrBgEFBQcwAYYoaHR0cDovL29jc3AuYXBwbGUuY29tL29jc3AwMy1hcHBsZXJv\n"
"b3RjYTAuBgNVHR8EJzAlMCOgIaAfhh1odHRwOi8vY3JsLmFwcGxlLmNvbS9yb290\n"
"LmNybDAdBgNVHQ4EFgQUGYuXjUpbYXhX9KVcNRKKOQjjsHUwDgYDVR0PAQH/BAQD\n"
"AgEGMBAGCiqGSIb3Y2QGAgEEAgUAMA0GCSqGSIb3DQEBCwUAA4IBAQBaxDWi2eYK\n"
"nlKiAIIid81yL5D5Iq8UJcyqCkJgksK9dR3rTMoV5X5rQBBe+1tFdA3wen2Ikc7e\n"
"Y4tCidIY30GzWJ4GCIdI3UCvI9Xt6yxg5eukfxzpnIPWlF9MYjmKTq4TjX1DuNxe\n"
"rL4YQPLmDyxdE5Pxe2WowmhI3v+0lpsM+zI2np4NlV84CouW0hJst4sLjtc+7G8B\n"
"qs5NRWDbhHFmYuUZZTDNiv9FU/tu+4h3Q8NIY/n3UbNyXnniVs+8u4S5OFp4rhFI\n"
"UrsNNYuU3sx0mmj1SWCUrPKosxWGkNDMMEOG0+VwAlG0gcCol9Tq6rCMCUDvOJOy\n"
"zSID62dDZchF\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG6 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIDFjCCApygAwIBAgIUIsGhRwp0c2nvU4YSycafPTjzbNcwCgYIKoZIzj0EAwMw\n"
"ZzEbMBkGA1UEAwwSQXBwbGUgUm9vdCBDQSAtIEczMSYwJAYDVQQLDB1BcHBsZSBD\n"
"ZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTETMBEGA1UECgwKQXBwbGUgSW5jLjELMAkG\n"
"A1UEBhMCVVMwHhcNMjEwMzE3MjAzNzEwWhcNMzYwMzE5MDAwMDAwWjB1MUQwQgYD\n"
"VQQDDDtBcHBsZSBXb3JsZHdpZGUgRGV2ZWxvcGVyIFJlbGF0aW9ucyBDZXJ0aWZp\n"
"Y2F0aW9uIEF1dGhvcml0eTELMAkGA1UECwwCRzYxEzARBgNVBAoMCkFwcGxlIElu\n"
"Yy4xCzAJBgNVBAYTAlVTMHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEbsQKC94PrlWm\n"
"ZXnXgtxzdVJL8T0SGYngDRGpngn3N6PT8JMEb7FDi4bBmPhCnZ3/sq6PF/cGcKXW\n"
"sL5vOteRhyJ45x3ASP7cOB+aao90fcpxSv/EZFbniAbNgZGhIhpIo4H6MIH3MBIG\n"
"A1UdEwEB/wQIMAYBAf8CAQAwHwYDVR0jBBgwFoAUu7DeoVgziJqkipnevr3rr9rL\n"
"JKswRgYIKwYBBQUHAQEEOjA4MDYGCCsGAQUFBzABhipodHRwOi8vb2NzcC5hcHBs\n"
"ZS5jb20vb2NzcDAzLWFwcGxlcm9vdGNhZzMwNwYDVR0fBDAwLjAsoCqgKIYmaHR0\n"
"cDovL2NybC5hcHBsZS5jb20vYXBwbGVyb290Y2FnMy5jcmwwHQYDVR0OBBYEFD8v\n"
"lCNR01DJmig97bB85c+lkGKZMA4GA1UdDwEB/wQEAwIBBjAQBgoqhkiG92NkBgIB\n"
"BAIFADAKBggqhkjOPQQDAwNoADBlAjBAXhSq5IyKogMCPtw490BaB677CaEGJXuf\n"
"QB/EqZGd6CSjiCtOnuMTbXVXmxxcxfkCMQDTSPxarZXvNrkxU3TkUMI33yzvFVVR\n"
"T4wxWJC994OsdcZ4+RGNsYDyR5gmdr0nDGg=\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG7 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEVTCCAz2gAwIBAgIUNBhY/wH+Bj+O8Z8f6TwBtMFG/8kwDQYJKoZIhvcNAQEF\n"
"BQAwYjELMAkGA1UEBhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xJjAkBgNVBAsT\n"
"HUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MRYwFAYDVQQDEw1BcHBsZSBS\n"
"b290IENBMB4XDTIyMTExNzIwNDA1M1oXDTIzMTExNzIwNDA1MlowdTELMAkGA1UE\n"
"BhMCVVMxEzARBgNVBAoMCkFwcGxlIEluYy4xCzAJBgNVBAsMAkc3MUQwQgYDVQQD\n"
"DDtBcHBsZSBXb3JsZHdpZGUgRGV2ZWxvcGVyIFJlbGF0aW9ucyBDZXJ0aWZpY2F0\n"
"aW9uIEF1dGhvcml0eTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKyu\n"
"0dO2irEbKJWt3lFRTD8z4U5cr7P8AtJlTyrUdGiMdRdlzyjkSAmYcVIyLBZOeI6S\n"
"VmSp3YvN4tTHO6ISRTcCGWJkL39hxtNZIr+r+RSj7baembov8bHcMEJPtrayxnSq\n"
"Yla77UQ2D9HlIHSTVzpdntwB/HhvaRY1w24Bwp5y1HE2sXYJer4NKpfxsF4LGxKt\n"
"K6sH32Mt9YjpMhKiVVhDdjw9F4AfKduxqZ+rlgWdFdzd204P5xN8WisuAkH27npq\n"
"tnNg95cZFIuVMziT2gAlNq5VWnyf+fRiBAd06R2nlVcjrCsk2mRPKHLplrAIPIgb\n"
"FGND14mumMHyLY7jUSUCAwEAAaOB7zCB7DASBgNVHRMBAf8ECDAGAQH/AgEAMB8G\n"
"A1UdIwQYMBaAFCvQaUeUdgn+9GuNLkCm90dNfwheMEQGCCsGAQUFBwEBBDgwNjA0\n"
"BggrBgEFBQcwAYYoaHR0cDovL29jc3AuYXBwbGUuY29tL29jc3AwMy1hcHBsZXJv\n"
"b3RjYTAuBgNVHR8EJzAlMCOgIaAfhh1odHRwOi8vY3JsLmFwcGxlLmNvbS9yb290\n"
"LmNybDAdBgNVHQ4EFgQUXUIQbBu7x1KXTkS9Eye5OhJ3gyswDgYDVR0PAQH/BAQD\n"
"AgEGMBAGCiqGSIb3Y2QGAgEEAgUAMA0GCSqGSIb3DQEBBQUAA4IBAQBSowgpE2W3\n"
"tR/mNAPt9hh3vD3KJ7Vw7OxsM0v2mSWUB54hMwNq9X0KLivfCKmC3kp/4ecLSwW4\n"
"J5hJ3cEMhteBZK6CnMRF8eqPHCIw46IlYUSJ/oV6VvByknwMRFQkt7WknybwMvlX\n"
"nWp5bEDtDzQGBkL/2A4xZW3mLgHZBr/Fyg2uR9QFF4g86ZzkGWRtipStEdwB9uV4\n"
"r63ocNcNXYE+RiosriShx9Lgfb8d9TZrxd6pCpqAsRFesmR+s8FXzMJsWZm39LDd\n"
"MdpI1mqB7rKLUDUW5udccWJusPJR4qht+CrLaHPGpsQaQ0kBPqmpAIqGbIOI0lxw\n"
"V3ra+HbMGdWw\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleDevCACertG8 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEVTCCAz2gAwIBAgIUVLULr3kNjX+Mr2hMVi9QaQoaul8wDQYJKoZIhvcNAQEF\n"
"BQAwYjELMAkGA1UEBhMCVVMxEzARBgNVBAoTCkFwcGxlIEluYy4xJjAkBgNVBAsT\n"
"HUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MRYwFAYDVQQDEw1BcHBsZSBS\n"
"b290IENBMB4XDTIzMDYyMDIzMzcxNVoXDTI1MDEyNDAwMDAwMFowdTELMAkGA1UE\n"
"BhMCVVMxEzARBgNVBAoMCkFwcGxlIEluYy4xCzAJBgNVBAsMAkc4MUQwQgYDVQQD\n"
"DDtBcHBsZSBXb3JsZHdpZGUgRGV2ZWxvcGVyIFJlbGF0aW9ucyBDZXJ0aWZpY2F0\n"
"aW9uIEF1dGhvcml0eTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANBA\n"
"ENQI+VIhY088aPfUnIICjINovLeNf4jnQk0s7yKlwonevQzXTWFQLTnkMHOl0tVo\n"
"mjPy79kqrS4fA7r4pfFCC1cuRsbQWNNwX/eyN+9qHz6/iTnCrf71BftYljHIhyzV\n"
"I7p1sCz1q6C68iAMTOskY2npIkDwjlhb3mR7iRtREgTgF7JZzd/x586vLDLoacHQ\n"
"CH4dokdz0Us7/bmF3EenKIJ5KUiJAijiwewsH1uG/Ni2y3HAcwFL/AUREWwBAzRa\n"
"9oHCXh98FA7eP2shy0/112HmhAOSvOclKZ7NWwzB2+PEOtl2V6wvOBQZyLexolVP\n"
"X06OGVmp2v1y2rAEIQUCAwEAAaOB7zCB7DASBgNVHRMBAf8ECDAGAQH/AgEAMB8G\n"
"A1UdIwQYMBaAFCvQaUeUdgn+9GuNLkCm90dNfwheMEQGCCsGAQUFBwEBBDgwNjA0\n"
"BggrBgEFBQcwAYYoaHR0cDovL29jc3AuYXBwbGUuY29tL29jc3AwMy1hcHBsZXJv\n"
"b3RjYTAuBgNVHR8EJzAlMCOgIaAfhh1odHRwOi8vY3JsLmFwcGxlLmNvbS9yb290\n"
"LmNybDAdBgNVHQ4EFgQUtb28gMQM4zik9LetI7PvRM65WoUwDgYDVR0PAQH/BAQD\n"
"AgEGMBAGCiqGSIb3Y2QGAgEEAgUAMA0GCSqGSIb3DQEBBQUAA4IBAQBMs+t6OZRK\n"
"lWb6FjHqDYqPXUI4xgfN6MkirPwIQn5fk18xKqgiwXYZK+6ucum9Vs9JJJII980Z\n"
"dcP5GicNDtwpjT+226VPTHLEYJGJEX4klUMiYGe83/+r5TwWF52CFE6d9HX+ULmt\n"
"BbK4efaV1hDl9lP0zyPmdw/suEtp+OKeAjHZjtnKvmNeX+Ggac7BzW5Jo3hhrzk8\n"
"aksKNCVk1TC1PKvdEYE5cejAw1iAERAaEdLCvFnwitk1c8DmbeTJfWIUPoICqRBp\n"
"N3lhb/BGlD419ausY9DYXllXadG4S25d1F8TnHBOJRHcJCweFp6WWgTtRe467mdd\n"
"j8OGsPVMH2gQ\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleRootCACert = ""
"-----BEGIN CERTIFICATE-----\n"
"MIIEuzCCA6OgAwIBAgIBAjANBgkqhkiG9w0BAQUFADBiMQswCQYDVQQGEwJVUzET\n"
"MBEGA1UEChMKQXBwbGUgSW5jLjEmMCQGA1UECxMdQXBwbGUgQ2VydGlmaWNhdGlv\n"
"biBBdXRob3JpdHkxFjAUBgNVBAMTDUFwcGxlIFJvb3QgQ0EwHhcNMDYwNDI1MjE0\n"
"MDM2WhcNMzUwMjA5MjE0MDM2WjBiMQswCQYDVQQGEwJVUzETMBEGA1UEChMKQXBw\n"
"bGUgSW5jLjEmMCQGA1UECxMdQXBwbGUgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkx\n"
"FjAUBgNVBAMTDUFwcGxlIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw\n"
"ggEKAoIBAQDkkakJH5HbHkdQ6wXtXnmELes2oldMVeyLGYne+Uts9QerIjAC6Bg+\n"
"+FAJ039BqJj50cpmnCRrEdCju+QbKsMflZ56DKRHi1vUFjczy8QPTc4UadHJGXL1\n"
"XQ7Vf1+b8iUDulWPTV0N8WQ1IxVLFVkds5T39pyez1C6wVhQZ48ItCD3y6wsIG9w\n"
"tj8BMIy3Q88PnT3zK0koGsj+zrW5DtleHNbLPbU6rfQPDgCSC7EhFi501TwN22IW\n"
"q6NxkkdTVcGvL0Gz+PvjcM3mo0xFfh9Ma1CWQYnEdGILEINBhzOKgbEwWOxaBDKM\n"
"aLOPHd5lc/9nXmW8Sdh2nzMUZaF3lMktAgMBAAGjggF6MIIBdjAOBgNVHQ8BAf8E\n"
"BAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUK9BpR5R2Cf70a40uQKb3\n"
"R01/CF4wHwYDVR0jBBgwFoAUK9BpR5R2Cf70a40uQKb3R01/CF4wggERBgNVHSAE\n"
"ggEIMIIBBDCCAQAGCSqGSIb3Y2QFATCB8jAqBggrBgEFBQcCARYeaHR0cHM6Ly93\n"
"d3cuYXBwbGUuY29tL2FwcGxlY2EvMIHDBggrBgEFBQcCAjCBthqBs1JlbGlhbmNl\n"
"IG9uIHRoaXMgY2VydGlmaWNhdGUgYnkgYW55IHBhcnR5IGFzc3VtZXMgYWNjZXB0\n"
"YW5jZSBvZiB0aGUgdGhlbiBhcHBsaWNhYmxlIHN0YW5kYXJkIHRlcm1zIGFuZCBj\n"
"b25kaXRpb25zIG9mIHVzZSwgY2VydGlmaWNhdGUgcG9saWN5IGFuZCBjZXJ0aWZp\n"
"Y2F0aW9uIHByYWN0aWNlIHN0YXRlbWVudHMuMA0GCSqGSIb3DQEBBQUAA4IBAQBc\n"
"NplMLXi37Yyb3PN3m/J20ncwT8EfhYOFG5k9RzfyqZtAjizUsZAS2L70c5vu0mQP\n"
"y3lPNNiiPvl4/2vIB+x9OYOLUyDTOMSxv5pPCmv/K/xZpwUJfBdAVhEedNO3iyM7\n"
"R6PVbyTi69G3cN8PReEnyvFteO3ntRcXqNx+IjXKJdXZD9Zr1KIkIxH3oayPc4Fg\n"
"xhtbCS+SsvhESPBgOJ4V9T0mZyCKM2r3DYLP3uujL/lTaltkwGMzd/c6ByxW69oP\n"
"IQ7aunMZT7XZNn/Bh1XZp5m5MkL72NVxnn6hUrcbvZNCJBIqxw8dtk2cXmPIS4AX\n"
"UKqK1drk/NAJBzewdXUh\n"
"-----END CERTIFICATE-----\n";

const char* ZSignAsset::s_szAppleRootCACertG3 = ""
"-----BEGIN CERTIFICATE-----\n"
"MIICQzCCAcmgAwIBAgIILcX8iNLFS5UwCgYIKoZIzj0EAwMwZzEbMBkGA1UEAwwS\n"
"QXBwbGUgUm9vdCBDQSAtIEczMSYwJAYDVQQLDB1BcHBsZSBDZXJ0aWZpY2F0aW9u\n"
"IEF1dGhvcml0eTETMBEGA1UECgwKQXBwbGUgSW5jLjELMAkGA1UEBhMCVVMwHhcN\n"
"MTQwNDMwMTgxOTA2WhcNMzkwNDMwMTgxOTA2WjBnMRswGQYDVQQDDBJBcHBsZSBS\n"
"b290IENBIC0gRzMxJjAkBgNVBAsMHUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9y\n"
"aXR5MRMwEQYDVQQKDApBcHBsZSBJbmMuMQswCQYDVQQGEwJVUzB2MBAGByqGSM49\n"
"AgEGBSuBBAAiA2IABJjpLz1AcqTtkyJygRMc3RCV8cWjTnHcFBbZDuWmBSp3ZHtf\n"
"TjjTuxxEtX/1H7YyYl3J6YRbTzBPEVoA/VhYDKX1DyxNB0cTddqXl5dvMVztK517\n"
"IDvYuVTZXpmkOlEKMaNCMEAwHQYDVR0OBBYEFLuw3qFYM4iapIqZ3r6966/ayySr\n"
"MA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMAoGCCqGSM49BAMDA2gA\n"
"MGUCMQCD6cHEFl4aXTQY2e3v9GwOAEZLuN+yRhHFD/3meoyhpmvOwgPUnPWTxnS4\n"
"at+qIxUCMG1mihDK1A3UT82NQz60imOlM27jbdoXt2QfyFMm+YhidDkLF1vLUagM\n"
"6BgD56KyKA==\n"
"-----END CERTIFICATE-----\n";

ZSignAsset::OpenSSLInit::OpenSSLInit()
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
#endif
}

bool ZSignAsset::CMSError()
{
	ERR_print_errors_fp(stdout);
	return false;
}

void* ZSignAsset::GenerateASN1Type(const string& value)
{
	long errline = -1;
	char* genstr = NULL;
	BIO* ldapbio = BIO_new(BIO_s_mem());
	CONF* cnf = NCONF_new(NULL);

	if (cnf == NULL) {
		ZLog::Error(">>> NCONF_new failed\n");
		BIO_free(ldapbio);
	}
	string a = "asn1=SEQUENCE:A\n[A]\nC=OBJECT:sha256\nB=FORMAT:HEX,OCT:" + value + "\n";
	int code = BIO_puts(ldapbio, a.c_str());
	if (NCONF_load_bio(cnf, ldapbio, &errline) <= 0) {
		BIO_free(ldapbio);
		NCONF_free(cnf);
		ZLog::PrintV(">>> NCONF_load_bio failed %d\n", errline);
	}
	BIO_free(ldapbio);
	genstr = NCONF_get_string(cnf, "default", "asn1");

	if (genstr == NULL) {
		ZLog::Error(">>> NCONF_get_string failed\n");
		NCONF_free(cnf);
	}
	ASN1_TYPE* ret = ASN1_generate_nconf(genstr, cnf);
	NCONF_free(cnf);
	return ret;
}

const char* ZSignAsset::WWDRIntermediatePEM(unsigned long uIssuerHash)
{
	switch (uIssuerHash) {
		case 0x817d2f7a: return s_szAppleDevCACert; //G1
		case 0x975904ef: return s_szAppleDevCACertG2;
		case 0x9b16b75c: return s_szAppleDevCACertG3;
		case 0x1eaac8b7: return s_szAppleDevCACertG4;
		case 0x12dfbd5d: return s_szAppleDevCACertG5;
		case 0xf8ecc621: return s_szAppleDevCACertG6;
		case 0xdbc90301: return s_szAppleDevCACertG7; //expired 2023-11; leaves it issued are expired too
		case 0x3da0a3ad: return s_szAppleDevCACertG8; //expired 2025-01; leaves it issued are expired too
	}
	return NULL;
}

// Parses a PEM certificate and appends it to certs, which takes ownership.
static bool AppendPEMCert(STACK_OF(X509)* certs, const char* szPEM)
{
	BIO* bio = BIO_new_mem_buf(szPEM, (int)strlen(szPEM));
	if (!bio) {
		return false;
	}
	X509* cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
	BIO_free(bio);
	if (!cert) {
		return false;
	}
	if (!sk_X509_push(certs, cert)) {
		X509_free(cert);
		return false;
	}
	return true;
}

static bool StackContainsIssuerOf(STACK_OF(X509)* certs, X509* cert)
{
	for (int i = 0; i < sk_X509_num(certs); i++) {
		if (X509_V_OK == X509_check_issued(sk_X509_value(certs, i), cert)) {
			return true;
		}
	}
	return false;
}

bool ZSignAsset::GenerateCMS(void* pscert, void* pspkey, const string& strCDHashData, const string& strCDHashesPlist, const string& strCodeDirectorySlotSHA1, const string& strAltnateCodeDirectorySlot256, string& strCMSOutput)
{
	if (!pscert || !pspkey) {
		return CMSError();
	}

	X509* scert = (X509*)pscert;
	EVP_PKEY* spkey = (EVP_PKEY*)pspkey;

	STACK_OF(X509)* otherCerts = sk_X509_new_null();
	if (!otherCerts) {
		return CMSError();
	}

	// Prefer the CA chain shipped inside the input p12, but only when it actually
	// contains the leaf's issuer; a p12 can carry an incomplete or unrelated chain
	// (e.g. a root-only export), which must not shadow the embedded intermediates.
	STACK_OF(X509)* caCerts = (STACK_OF(X509)*)m_caCerts;
	if (NULL != caCerts && StackContainsIssuerOf(caCerts, scert)) {
		for (int i = 0; i < sk_X509_num(caCerts); i++) {
			X509* cert = sk_X509_value(caCerts, i);
			if (!X509_up_ref(cert)) {
				return CMSError();
			}
			if (!sk_X509_push(otherCerts, cert)) {
				X509_free(cert);
				return CMSError();
			}
		}
	} else {
		// Fall back to the embedded WWDR intermediate matching the leaf's issuer.
		unsigned long issuerHash = X509_issuer_name_hash(scert);
		const char* szIssuerCert = WWDRIntermediatePEM(issuerHash);
		if (NULL == szIssuerCert) {
			ZLog::ErrorV(">>> Unknown issuer hash 0x%08lx! No embedded WWDR intermediate matches and the p12 carries no usable CA chain.\n", issuerHash);
			return false;
		}
		if (!AppendPEMCert(otherCerts, szIssuerCert)) {
			return CMSError();
		}
	}

	// Terminate the chain with the Apple root that actually issued one of its
	// members (WWDR G2/G6 chain to Apple Root CA - G3, the others to Apple Root
	// CA), skipping roots already present, to match Apple codesign output.
	static const char* arrRootPEMs[] = { s_szAppleRootCACert, s_szAppleRootCACertG3 };
	for (size_t r = 0; r < sizeof(arrRootPEMs) / sizeof(arrRootPEMs[0]); r++) {
		BIO* bio = BIO_new_mem_buf(arrRootPEMs[r], (int)strlen(arrRootPEMs[r]));
		X509* root = (NULL != bio) ? PEM_read_bio_X509(bio, NULL, 0, NULL) : NULL;
		BIO_free(bio);
		if (!root) {
			return CMSError();
		}

		bool bIssuedChain = false;
		bool bPresent = false;
		for (int i = 0; i < sk_X509_num(otherCerts); i++) {
			X509* cert = sk_X509_value(otherCerts, i);
			if (0 == X509_cmp(root, cert)) {
				bPresent = true;
			} else if (X509_V_OK == X509_check_issued(root, cert)) {
				bIssuedChain = true;
			}
		}
		if (bIssuedChain && !bPresent) {
			if (!sk_X509_push(otherCerts, root)) {
				X509_free(root);
				return CMSError();
			}
		} else {
			X509_free(root);
		}
	}

	BIO* in = BIO_new_mem_buf(strCDHashData.c_str(), (int)strCDHashData.size());
	if (!in) {
		return CMSError();
	}

	int nFlags = CMS_PARTIAL | CMS_DETACHED | CMS_NOSMIMECAP | CMS_BINARY;
	CMS_ContentInfo* cms = CMS_sign(NULL, NULL, otherCerts, NULL, nFlags);
	if (!cms) {
		return CMSError();
	}

	CMS_SignerInfo* si = CMS_add1_signer(cms, scert, spkey, EVP_sha256(), nFlags);
	//    CMS_add1_signer(cms, NULL, NULL, EVP_sha1(), nFlags);
	if (!si) {
		return CMSError();
	}

	// add plist
	ASN1_OBJECT* obj = OBJ_txt2obj("1.2.840.113635.100.9.1", 1);
	if (!obj) {
		return CMSError();
	}

	int addHashPlist = CMS_signed_add1_attr_by_OBJ(si, obj, 0x4, strCDHashesPlist.c_str(), (int)strCDHashesPlist.size());

	if (!addHashPlist) {
		return CMSError();
	}

	// add CDHashes
	static const char hex_upper[] = "0123456789ABCDEF";
	string sha256;
	sha256.reserve(strAltnateCodeDirectorySlot256.size() * 2);
	for (size_t i = 0; i < strAltnateCodeDirectorySlot256.size(); i++) {
		uint8_t c = (uint8_t)strAltnateCodeDirectorySlot256[i];
		sha256 += hex_upper[c >> 4];
		sha256 += hex_upper[c & 0x0F];
	}

	ASN1_OBJECT* obj2 = OBJ_txt2obj("1.2.840.113635.100.9.2", 1);
	if (!obj2) {
		return CMSError();
	}

	X509_ATTRIBUTE* attr = X509_ATTRIBUTE_new();
	X509_ATTRIBUTE_set1_object(attr, obj2);

	ASN1_TYPE* type_256 = (ASN1_TYPE*)GenerateASN1Type(sha256);
	X509_ATTRIBUTE_set1_data(attr, V_ASN1_SEQUENCE,
		type_256->value.asn1_string->data, type_256->value.asn1_string->length);
	int addHashSHA = CMS_signed_add1_attr(si, attr);
	if (!addHashSHA) {
		return CMSError();
	}

	if (!CMS_final(cms, in, NULL, nFlags)) {
		return CMSError();
	}

	BIO* out = BIO_new(BIO_s_mem());
	if (!out) {
		return CMSError();
	}

	//PEM_write_bio_CMS(out, cms);
	if (!i2d_CMS_bio(out, cms)) {
		return CMSError();
	}

	BUF_MEM* bptr = NULL;
	BIO_get_mem_ptr(out, &bptr);
	if (!bptr) {
		return CMSError();
	}

	strCMSOutput.clear();
	strCMSOutput.append(bptr->data, bptr->length);
	ASN1_TYPE_free(type_256);
	sk_X509_pop_free(otherCerts, X509_free); // CMS_sign holds its own references
	return (!strCMSOutput.empty());
}

bool ZSignAsset::GetCMSContent(const string& strCMSDataInput, string& strContentOutput)
{
	if (strCMSDataInput.empty()) {
		return false;
	}

	BIO* in = BIO_new(BIO_s_mem());
	OPENSSL_assert((size_t)BIO_write(in, strCMSDataInput.data(), (int)strCMSDataInput.size()) == strCMSDataInput.size());
	CMS_ContentInfo* cms = d2i_CMS_bio(in, NULL);
	if (!cms) {
		return CMSError();
	}

	ASN1_OCTET_STRING** pos = CMS_get0_content(cms);
	if (!pos) {
		return CMSError();
	}

	if (!(*pos)) {
		return CMSError();
	}

	strContentOutput.clear();
	strContentOutput.append((const char*)(*pos)->data, (*pos)->length);
	return (!strContentOutput.empty());
}

bool ZSignAsset::GetCertSubjectCN(void* pcert, string& strSubjectCN)
{
	if (!pcert) {
		return CMSError();
	}

	X509* cert = (X509*)pcert;

	X509_NAME* name = X509_get_subject_name(cert);

	int common_name_loc = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
	if (common_name_loc < 0) {
		return CMSError();
	}

	X509_NAME_ENTRY* common_name_entry = X509_NAME_get_entry(name, common_name_loc);
	if (common_name_entry == NULL) {
		return CMSError();
	}

	ASN1_STRING* common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
	if (common_name_asn1 == NULL) {
		return CMSError();
	}

	strSubjectCN.clear();
	strSubjectCN.append((const char*)common_name_asn1->data, common_name_asn1->length);
	return (!strSubjectCN.empty());
}

bool ZSignAsset::GetCertSubjectCN(const string& strCertData, string& strSubjectCN)
{
	if (strCertData.empty()) {
		return false;
	}

	BIO* bcert = BIO_new_mem_buf(strCertData.c_str(), (int)strCertData.size());
	if (!bcert) {
		return CMSError();
	}

	X509* cert = PEM_read_bio_X509(bcert, NULL, 0, NULL);
	if (!cert) {
		return CMSError();
	}

	return GetCertSubjectCN(cert, strSubjectCN);
}

void ZSignAsset::ParseCertSubject(const string& strSubject, jvalue& jvSubject)
{
	vector<string> arrNodes;
	ZUtil::StringSplit(strSubject, "/", arrNodes);
	for (size_t i = 0; i < arrNodes.size(); i++) {
		vector<string> arrLines;
		ZUtil::StringSplit(arrNodes[i], "=", arrLines);
		if (2 == arrLines.size()) {
			jvSubject[arrLines[0]] = arrLines[1];
		}
	}
}

string ZSignAsset::ASN1_TIMEtoString(const void* time)
{
	BIO* out = BIO_new(BIO_s_mem());
	if (!out) {
		CMSError();
		return "";
	}

	ASN1_TIME_print(out, (const ASN1_TIME*)time);
	BUF_MEM* bptr = NULL;
	BIO_get_mem_ptr(out, &bptr);
	if (!bptr) {
		CMSError();
		return "";
	}
	string strTime;
	strTime.append(bptr->data, bptr->length);
	return strTime;
}

bool ZSignAsset::GetCertInfo(void* pcert, jvalue & jvCertInfo)
{
	if (!pcert) {
		return CMSError();
	}

	X509* cert = (X509*)pcert;

	jvCertInfo["Version"] = (int)X509_get_version(cert);

	ASN1_INTEGER* asn1_i = X509_get_serialNumber(cert);
	if (asn1_i) {
		BIGNUM* bignum = ASN1_INTEGER_to_BN(asn1_i, NULL);
		if (bignum) {
			jvCertInfo["SerialNumber"] = BN_bn2hex(bignum);
		}
	}

	jvCertInfo["SignatureAlgorithm"] = OBJ_nid2ln(X509_get_signature_nid(cert));

	EVP_PKEY* pubkey = X509_get_pubkey(cert);
	int type = EVP_PKEY_id(pubkey);
	jvCertInfo["PublicKey"]["Algorithm"] = OBJ_nid2ln(type);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	jvCertInfo["Validity"]["NotBefore"] = ASN1_TIMEtoString(X509_get_notBefore(cert));
	jvCertInfo["Validity"]["NotAfter"] = ASN1_TIMEtoString(X509_get_notAfter(cert));
#else
	jvCertInfo["Validity"]["NotBefore"] = ASN1_TIMEtoString(X509_get0_notBefore(cert));
	jvCertInfo["Validity"]["NotAfter"] = ASN1_TIMEtoString(X509_get0_notAfter(cert));
#endif

	string strIssuer = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
	string strSubject = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);

	ParseCertSubject(strIssuer, jvCertInfo["Issuer"]);
	ParseCertSubject(strSubject, jvCertInfo["Subject"]);

	return (!strIssuer.empty() && !strSubject.empty());
}

bool ZSignAsset::GetCMSInfo(uint8_t * pCMSData, uint32_t uCMSLength, jvalue & jvOutput)
{
	BIO* in = BIO_new(BIO_s_mem());
	OPENSSL_assert((size_t)BIO_write(in, pCMSData, uCMSLength) == uCMSLength);
	CMS_ContentInfo* cms = d2i_CMS_bio(in, NULL);
	if (!cms) {
		return CMSError();
	}

	int detached = CMS_is_detached(cms);
	jvOutput["detached"] = detached;

	const ASN1_OBJECT* obj = CMS_get0_type(cms);
	const char* sn = OBJ_nid2ln(OBJ_obj2nid(obj));
	jvOutput["contentType"] = sn;

	ASN1_OCTET_STRING** pos = CMS_get0_content(cms);
	if (pos) {
		if ((*pos)) {
			jbase64 b64;
			jvOutput["content"] = b64.encode((const char*)(*pos)->data, (*pos)->length);
		}
	}

	STACK_OF(X509)* certs = CMS_get1_certs(cms);
	for (int i = 0; i < sk_X509_num(certs); i++) {
		jvalue jvCertInfo;
		if (GetCertInfo(sk_X509_value(certs, i), jvCertInfo)) {
			jvOutput["certs"].push_back(jvCertInfo);
		}
	}

	STACK_OF(CMS_SignerInfo)* sis = CMS_get0_SignerInfos(cms);
	for (int i = 0; i < sk_CMS_SignerInfo_num(sis); i++) {
		CMS_SignerInfo* si = sk_CMS_SignerInfo_value(sis, i);
		//int CMS_SignerInfo_get0_signer_id(CMS_SignerInfo *si, ASN1_OCTET_STRING **keyid, X509_NAME **issuer, ASN1_INTEGER **sno);

		int nSignedAttsCount = CMS_signed_get_attr_count(si);
		for (int j = 0; j < nSignedAttsCount; j++) {
			X509_ATTRIBUTE* attr = CMS_signed_get_attr(si, j);
			if (!attr) {
				continue;
			}
			int nCount = X509_ATTRIBUTE_count(attr);
			if (nCount <= 0) {
				continue;
			}

			ASN1_OBJECT* obj = X509_ATTRIBUTE_get0_object(attr);
			if (!obj) {
				continue;
			}

			char txtobj[128] = { 0 };
			OBJ_obj2txt(txtobj, 128, obj, 1);

			if (0 == strcmp("1.2.840.113549.1.9.3", txtobj)) { //V_ASN1_OBJECT
				ASN1_TYPE* av = X509_ATTRIBUTE_get0_type(attr, 0);
				if (NULL != av) {
					jvOutput["attrs"]["ContentType"]["obj"] = txtobj;
					jvOutput["attrs"]["ContentType"]["data"] = OBJ_nid2ln(OBJ_obj2nid(av->value.object));
				}
			} else if (0 == strcmp("1.2.840.113549.1.9.4", txtobj)) { //V_ASN1_OCTET_STRING
				ASN1_TYPE* av = X509_ATTRIBUTE_get0_type(attr, 0);
				if (NULL != av) {
					static const char hex_lower[] = "0123456789abcdef";
					string strSHASum;
					strSHASum.reserve(av->value.octet_string->length * 2);
					for (int m = 0; m < av->value.octet_string->length; m++) {
						uint8_t c = (uint8_t)av->value.octet_string->data[m];
						strSHASum += hex_lower[c >> 4];
						strSHASum += hex_lower[c & 0x0F];
					}
					jvOutput["attrs"]["MessageDigest"]["obj"] = txtobj;
					jvOutput["attrs"]["MessageDigest"]["data"] = strSHASum;
				}
			} else if (0 == strcmp("1.2.840.113549.1.9.5", txtobj)) { //V_ASN1_UTCTIME
				ASN1_TYPE* av = X509_ATTRIBUTE_get0_type(attr, 0);
				if (NULL != av) {
					BIO* mem = BIO_new(BIO_s_mem());
					ASN1_UTCTIME_print(mem, av->value.utctime);
					BUF_MEM* bptr = NULL;
					BIO_get_mem_ptr(mem, &bptr);
					BIO_set_close(mem, BIO_NOCLOSE);
					string strTime;
					strTime.append(bptr->data, bptr->length);
					BIO_free_all(mem);

					jvOutput["attrs"]["SigningTime"]["obj"] = txtobj;
					jvOutput["attrs"]["SigningTime"]["data"] = strTime;
				}
			} else if (0 == strcmp("1.2.840.113635.100.9.2", txtobj)) { //V_ASN1_SEQUENCE
				jvOutput["attrs"]["CDHashes2"]["obj"] = txtobj;
				for (int m = 0; m < nCount; m++) {
					ASN1_TYPE* av = X509_ATTRIBUTE_get0_type(attr, m);
					if (NULL != av) {
						ASN1_STRING* s = av->value.sequence;

						BIO* mem = BIO_new(BIO_s_mem());

						ASN1_parse_dump(mem, s->data, s->length, 2, 0);
						BUF_MEM* bptr = NULL;
						BIO_get_mem_ptr(mem, &bptr);
						BIO_set_close(mem, BIO_NOCLOSE);
						string strData;
						strData.append(bptr->data, bptr->length);
						BIO_free_all(mem);

						string strSHASum;
						size_t pos1 = strData.find("[HEX DUMP]:");
						if (string::npos != pos1) {
							size_t pos2 = strData.find("\n", pos1);
							if (string::npos != pos2) {
								strSHASum = strData.substr(pos1 + 11, pos2 - pos1 - 11);
							}
						}
						transform(strSHASum.begin(), strSHASum.end(), strSHASum.begin(), ::tolower);
						jvOutput["attrs"]["CDHashes2"]["data"].push_back(strSHASum);
					}
				}
			} else if (0 == strcmp("1.2.840.113635.100.9.1", txtobj)) { //V_ASN1_OCTET_STRING
				ASN1_TYPE* av = X509_ATTRIBUTE_get0_type(attr, 0);
				if (NULL != av) {
					string strPList;
					strPList.append((const char*)av->value.octet_string->data, av->value.octet_string->length);
					jvOutput["attrs"]["CDHashes"]["obj"] = txtobj;
					jvOutput["attrs"]["CDHashes"]["data"] = strPList;
				}
			} else {
				ASN1_TYPE* av = X509_ATTRIBUTE_get0_type(attr, 0);
				if (NULL != av) {
					jvalue jvAttr;
					jvAttr["obj"] = txtobj;
					jvAttr["name"] = OBJ_nid2ln(OBJ_obj2nid(obj));
					jvAttr["type"] = av->type;
					jvAttr["count"] = nCount;
					jvOutput["attrs"]["unknown"].push_back(jvAttr);
				}
			}
		}
	}

	return true;
}

ZSignAsset::ZSignAsset()
{
	m_evpPKey = NULL;
	m_x509Cert = NULL;
	m_caCerts = NULL;
	m_bAdhoc = false;
	m_bSingleBinary = false;
	m_bSHA256Only = false;
}

bool ZSignAsset::Init(
	const string& strCertFile,
	const string& strPKeyFile, 
	const string& strProvFile, 
	const string& strEntitleFile, 
	const string& strPassword,
	bool bAdhoc,
	bool bSHA256Only,
	bool bSingleBinary)
{
	m_bAdhoc = bAdhoc;
	m_bSHA256Only = bSHA256Only;
	m_bSingleBinary = bSingleBinary;

	if (m_bAdhoc) {
		if (!strEntitleFile.empty()) {
			if (!ZFile::ReadFile(strEntitleFile.c_str(), m_strEntitleData)) {
				ZLog::Error(">>> Can't read entitlements file!\n");
				return false;
			}
		}
		return true;
	}

	ZFile::ReadFile(strProvFile.c_str(), m_strProvData);
	ZFile::ReadFile(strEntitleFile.c_str(), m_strEntitleData);
	if (m_strProvData.empty()) {
		ZLog::Error(">>> Can't find provision file!\n");
		return false;
	}

	jvalue jvProv;
	string strProvContent;
	if (GetCMSContent(m_strProvData, strProvContent)) {
		if (jvProv.read_plist(strProvContent)) {
			m_strApplicationId = jvProv["Entitlements"]["application-identifier"].as_cstr();
			m_strTeamId = jvProv["TeamIdentifier"][0].as_cstr();
			if (m_strEntitleData.empty()) {
				jvProv["Entitlements"].style_write_plist(m_strEntitleData);
			}
		}
	}

	if (m_strTeamId.empty()) {
		ZLog::Error(">>> Can't find TeamId!\n");
		return false;
	}

	X509* x509Cert = NULL;
	EVP_PKEY* evpPKey = NULL;
	BIO* bioPKey = BIO_new_file(strPKeyFile.c_str(), "rb");
	if (NULL != bioPKey) {
		evpPKey = PEM_read_bio_PrivateKey(bioPKey, NULL, NULL, (void*)strPassword.c_str());
		if (NULL == evpPKey) {
			BIO_reset(bioPKey);
			evpPKey = d2i_PrivateKey_bio(bioPKey, NULL);
			if (NULL == evpPKey) {
				BIO_reset(bioPKey);
				OSSL_PROVIDER_load(NULL, "legacy");
				PKCS12* p12 = d2i_PKCS12_bio(bioPKey, NULL);
				if (NULL != p12) {
					STACK_OF(X509)* caCerts = NULL;
					if (0 == PKCS12_parse(p12, strPassword.c_str(), &evpPKey, &x509Cert, &caCerts)) {
						CMSError();
					}
					// Keep the p12's own CA chain so GenerateCMS can embed the exact
					// intermediate(s) that issued the leaf, for any WWDR generation.
					m_caCerts = caCerts;
					PKCS12_free(p12);
				} else {
					CMSError();
				}
			}
		}
		BIO_free(bioPKey);
	}

	if (NULL == evpPKey) {
		ZLog::Error(">>> Can't load p12 or private key file. Please input the correct file and password!\n");
		return false;
	}

	if (NULL == x509Cert && !strCertFile.empty()) {
		BIO* bioCert = BIO_new_file(strCertFile.c_str(), "r");
		if (NULL != bioCert) {
			x509Cert = PEM_read_bio_X509(bioCert, NULL, 0, NULL);
			if (NULL == x509Cert) {
				BIO_reset(bioCert);
				x509Cert = d2i_X509_bio(bioCert, NULL);
			}
			BIO_free(bioCert);
		}
	}

	if (NULL != x509Cert) {
		if (!X509_check_private_key(x509Cert, evpPKey)) {
			X509_free(x509Cert);
			x509Cert = NULL;
		}
	}

	if (NULL == x509Cert) {
		for (size_t i = 0; i < jvProv["DeveloperCertificates"].size(); i++) {
			string strCertData = jvProv["DeveloperCertificates"][i].as_data();
			BIO* bioCert = BIO_new_mem_buf(strCertData.c_str(), (int)strCertData.size());
			if (NULL != bioCert) {
				x509Cert = d2i_X509_bio(bioCert, NULL);
				if (NULL != x509Cert) {
					if (X509_check_private_key(x509Cert, evpPKey)) {
						break;
					}
					X509_free(x509Cert);
					x509Cert = NULL;
				}
				BIO_free(bioCert);
			}
		}
	}

	if (NULL == x509Cert) {
		ZLog::Error(">>> Can't find paired certificate and private key!\n");
		return false;
	}

	if (!GetCertSubjectCN(x509Cert, m_strSubjectCN)) {
		ZLog::Error(">>> Can't find paired certificate subject common name!\n");
		return false;
	}

	m_evpPKey = evpPKey;
	m_x509Cert = x509Cert;
	return true;
}

bool ZSignAsset::GenerateCMS(const string& strCDHashData, const string& strCDHashesPlist, const string& strCodeDirectorySlotSHA1, const string& strAltnateCodeDirectorySlot256, string& strCMSOutput)
{
	return GenerateCMS((X509*)m_x509Cert, (EVP_PKEY*)m_evpPKey, strCDHashData, strCDHashesPlist, strCodeDirectorySlotSHA1, strAltnateCodeDirectorySlot256, strCMSOutput);
}

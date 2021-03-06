/*
Copyright 2015 Google Inc. All rights reserved.

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#include <CFNetwork/CFNetServices.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFStream.h>

#include <stdio.h>  // asprintf
#include <stdlib.h> // free

// TODO: Uncomment when implemented in Go.
//#include "_cgo_export.h"

// streamErrorToString converts a CFStreamError to a string.
char *streamErrorToString(CFStreamError *error) {
	const char *errorDomain;
	switch (error->domain) {
	case kCFStreamErrorDomainCustom:
		errorDomain = "custom";
		break;
	case kCFStreamErrorDomainPOSIX:
		errorDomain = "POSIX";
		break;
	case kCFStreamErrorDomainMacOSStatus:
		errorDomain = "MacOS status";
		break;
	default:
		errorDomain = "unknown";
		break;
	}

	char *err = NULL;
	asprintf(&err, "domain %s code %d", errorDomain, error->error);
	return err;
}

void registerCallback(CFNetServiceRef service, CFStreamError *streamError, void *info) {
	CFStringRef printerName = (CFStringRef)info;
	char *printerNameC = malloc(sizeof(char) * (CFStringGetLength(printerName) + 1));
	char *streamErrorC = streamErrorToString(streamError);
	char *error = NULL;
	asprintf(&error, "Error while announcing Bonjour service for printer %s: %s",
			printerNameC, streamErrorC);

	// TODO: Uncomment when implemented in Go.
	//logBonjourError(error);

	CFRelease(printerName);
	free(printerNameC);
	free(streamErrorC);
	free(error);
}

// startBonjour starts and returns a bonjour service.
//
// Returns a registered service. Returns NULL and sets err on failure.
CFNetServiceRef startBonjour(char *name, char *type, char *domain, int port, char *url, char *id, char *cs, char **err) {
	CFStringRef n = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
	CFStringRef t = CFStringCreateWithCString(NULL, type, kCFStringEncodingASCII);
	CFStringRef d = CFStringCreateWithCString(NULL, domain, kCFStringEncodingASCII);
	CFStringRef u = CFStringCreateWithCString(NULL, url, kCFStringEncodingASCII);
	CFStringRef i = CFStringCreateWithCString(NULL, id, kCFStringEncodingASCII);
	CFStringRef c = CFStringCreateWithCString(NULL, cs, kCFStringEncodingASCII);

	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(dict, CFSTR("txtvers"), CFSTR("1"));
	CFDictionarySetValue(dict, CFSTR("ty"), n);
	CFDictionarySetValue(dict, CFSTR("url"), u);
	CFDictionarySetValue(dict, CFSTR("type"), CFSTR("printer"));
	CFDictionarySetValue(dict, CFSTR("id"), i);
	CFDictionarySetValue(dict, CFSTR("cs"), c);
	CFDataRef txt = CFNetServiceCreateTXTDataWithDictionary(NULL, dict);

	CFNetServiceRef service = CFNetServiceCreate(NULL, d, t, n, port);
	CFNetServiceSetTXTData(service, txt);
	// context now owns n, and will release n when service is released.
	CFNetServiceClientContext context = {0, (void *) n, NULL, CFRelease, NULL};
	CFNetServiceSetClient(service, registerCallback, &context);
	CFNetServiceScheduleWithRunLoop(service, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

	CFOptionFlags options = kCFNetServiceFlagNoAutoRename;
	CFStreamError error;

	if (!CFNetServiceRegisterWithOptions(service, options, &error)) {
		char *errorString = streamErrorToString(&error);
		asprintf(err, "Failed to register Bonjour service: %s", errorString);
		free(errorString);
		CFRelease(service);
		service = NULL;
	}

	CFRelease(t);
	CFRelease(d);
	CFRelease(u);
	CFRelease(i);
	CFRelease(c);
	CFRelease(dict);
	CFRelease(txt);

	return service;
}

// stopBonjour stops service and frees associated resources.
void stopBonjour(CFNetServiceRef service) {
	CFNetServiceUnscheduleFromRunLoop(service, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
	CFNetServiceSetClient(service, NULL, NULL);
	CFNetServiceCancel(service);
	CFRelease(service);
}

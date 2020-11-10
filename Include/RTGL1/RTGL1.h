#pragma once

#ifdef __cplusplus
extern "C" {
#endif


	typedef enum RgResult
	{
		RG_SUCCESS = 0,
		RG_ERROR = 1
	} RgResult;

	typedef enum RgStructureType
	{
		RG_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
	} RgStructureType;

	typedef struct RgInstanceCreateInfo
	{
		RgStructureType sType;
		const char *name;
	} RgInstanceCreateInfo;




	void rgCreateInstance(RgInstanceCreateInfo *info);


#ifdef __cplusplus
}
#endif
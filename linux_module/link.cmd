SECTIONS
{
	_v3_devices :
	{
		__start__v3_devices = .;
		*(_v3_devices);
		__stop__v3_devices = .;

	}
	
/*	_v3_capsules :
	{
		__start__v3_capsules = .;
		*(_v3_capsules);
		__stop__v3_capsules = .;
	}
*/
	_v3_shdw_pg_impls :
	{
		__start__v3_shdw_pg_impls = .;
		*(_v3_shdw_pg_impls);
		__stop__v3_shdw_pg_impls = .;

	}
	_v3_extensions :
	{
		__start__v3_extensions = .;
		*(_v3_extensions);
		__stop__v3_extensions = .;

	}
}


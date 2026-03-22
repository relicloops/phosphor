import type { ComponentType } from '@relicloops/cathode';

type Loader<T> = () => Promise<T>;

export const asDefaultExport = <TModule, TProps = any>(
  loader: Loader<TModule>,
  pick: ( mod: TModule ) => ComponentType<TProps>
) => {
  return () => loader().then( mod => ( { default: pick( mod ) } ) );
};

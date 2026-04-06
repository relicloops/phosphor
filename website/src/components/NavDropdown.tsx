import { css } from '@relicloops/cathode';

/**
 * NavDropdown -- reusable hover dropdown for header navigation
 *
 * Renders a nav link that shows a dropdown menu on hover.
 * Fully configurable: label, href, items, and CSS class prefix.
 *
 * Usage:
 *   <NavDropdown
 *     label="Dashboard"
 *     href="/dashboard.html"
 *     items={[
 *       { label: 'Manual', href: '/dashboard-manual.html' },
 *       { label: 'Implementation', href: '/dashboard-implementation.html' },
 *     ]}
 *   />
 */

export interface NavDropdownItem {
  label: string;
  href: string;
}

interface NavDropdownProps {
  label: string;
  href: string;
  items: NavDropdownItem[];
  classPrefix?: string;
}

export const NavDropdown = ( props: NavDropdownProps ) => {

  css( './css/components/nav-dropdown.css' );

  const prefix = props.classPrefix || 'nav-drop';

  return (
    <li class={`${prefix}`}>
      <a href={props.href} class={`header__nav-link ${prefix}__trigger`}>
        {props.label}
      </a>
      <ul class={`${prefix}__menu`}>
        {props.items.map( item => (
          <li class={`${prefix}__menu-item`}>
            <a href={item.href} class={`${prefix}__menu-link`}>
              {item.label}
            </a>
          </li>
        ) )}
      </ul>
    </li>
  );
};

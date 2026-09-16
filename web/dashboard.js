import {createDashboard as createBaseDashboard} from './dashboard-base.js';
import {enhanceElectricalPath} from './electrical-path.js';

export function createDashboard(){
 const base=createBaseDashboard();
 return enhanceElectricalPath(base);
}
